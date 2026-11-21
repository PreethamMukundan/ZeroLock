// Copyright Preetham Mukundan (C) 2026


#include "Tempest/ZL_Tempest_IAmNumber10.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Kismet/KismetMathLibrary.h"
#include "Mover/ZeroMoverComponent.h"
#include "Tempest/ZL_Tempest_StormBall.h"
#include "TimerManager.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "Xayah/ZL_Xayah_ThrowAbility.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Tempest_IAmNumber10::UZL_Tempest_IAmNumber10()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ProjectileClass = AZL_Tempest_StormBall::StaticClass();
	AbilityInputID = EGASAbilityInputID::Ultimate;
	Slot = EGameplayAbilitySlot::UltimateSlot;
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_ABILITIES_BLOCKED);
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_TEMPEST_IAMNUMBER10);
	CooldownDuration = 100.0f;
	AbilityName = TEXT("I Am Number 10");
	AbilityDescription = TEXT("Leap up and hang in the air: left click throws a huge wind ball through enemies, three in all. It bursts on every enemy it passes and where it lands, pulling enemies in and leaving a wind dome for 3 s that damages and slows. Jump or the ult key ends it early.");
}

void UZL_Tempest_IAmNumber10::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEnding = false;
	bHolding = false;
	bConfirmBound = false;
	BallsFired = 0;
	PendingSpawns = 0;

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
	if (!Hero || !Mover || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), CastCue, Hero->GetActorLocation(), Hero->GetActorRotation());

	// The owner predicts the leap; every machine then switches to the hold mode so the Mover agrees.
	if (IsLocallyControlled())
	{
		Mover->RequestSafeAbilityMove(FVector::UpVector * LeapUpSpeed, FMath::Max(LeapDuration, 0.01f));
	}
	UAbilityTask_WaitDelay* LeapTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(LeapDuration, 0.01f));
	LeapTask->OnFinish.AddDynamic(this, &UZL_Tempest_IAmNumber10::OnLeapFinished);
	LeapTask->ReadyForActivation();

	if (!IsLocallyControlled())
	{
		// Remote server: every task waits for one packet of the client's spawn data, so they all start now.
		PendingSpawns = NumBalls;
		for (int32 Index = 0; Index < NumBalls && IsActive(); ++Index)
		{
			StartSpawnTask(Hero->GetActorLocation(), Hero->GetActorRotation());
		}
		// The client ends it (replicated); this only catches a client that never does.
		if (IsActive())
		{
			GetWorld()->GetTimerManager().SetTimer(ServerSafetyTimer, this, &UZL_Tempest_IAmNumber10::Stop, FireDelay + ConfirmWindow + 3.0f, false);
		}
		return;
	}

	PendingSpawns = NumBalls;
	WindowTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, FMath::Max(FireDelay + ConfirmWindow, 0.1f));
	WindowTask->OnProgress.AddDynamic(this, &UZL_Tempest_IAmNumber10::UpdateProgressionTimer);
	WindowTask->OnStarted.AddDynamic(this, &UZL_Tempest_IAmNumber10::StartProgressionTimer);
	WindowTask->OnEnd.AddDynamic(this, &UZL_Tempest_IAmNumber10::StopProgressionTimer);
	WindowTask->OnFinished.AddDynamic(this, &UZL_Tempest_IAmNumber10::OnWindowFinished);
	WindowTask->ReadyForActivation();

	UAbilityTask_WaitInputPress* Repress = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	Repress->OnPress.AddDynamic(this, &UZL_Tempest_IAmNumber10::OnRepressed);
	Repress->ReadyForActivation();

	JumpHandle = Hero->OnJumpInputPressed.AddUObject(this, &UZL_Tempest_IAmNumber10::OnJumpPressed);

	UAbilityTask_WaitDelay* FireDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(FireDelay, 0.01f));
	FireDelayTask->OnFinish.AddDynamic(this, &UZL_Tempest_IAmNumber10::OnFireDelayFinished);
	FireDelayTask->ReadyForActivation();
}

void UZL_Tempest_IAmNumber10::OnLeapFinished()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
	if (!Mover || !IsActive() || bEnding)
	{
		return;
	}
	if (IsLocallyControlled())
	{
		Mover->RequestSafeStop();
	}
	if (!HoldModeName.IsNone())
	{
		Mover->QueueNextMode(HoldModeName);
		bHolding = true;
	}
}

void UZL_Tempest_IAmNumber10::OnFireDelayFinished()
{
	ArmConfirm();
}

void UZL_Tempest_IAmNumber10::ArmConfirm()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !IsActive() || bEnding || bConfirmBound)
	{
		return;
	}
	// The ASC clears its confirm callbacks after every confirm, so this is re-bound per ball. While it's bound, the
	// weapon's fire timer skips, so the click only throws.
	ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &UZL_Tempest_IAmNumber10::OnConfirmPressed);
	bConfirmBound = true;
}

void UZL_Tempest_IAmNumber10::OnConfirmPressed()
{
	bConfirmBound = false;
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !IsActive() || bEnding || BallsFired >= NumBalls)
	{
		return;
	}

	const UCameraComponent* Camera = Hero->GetFollowCamera();
	const FVector AimDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : GetHeroAimDirection();
	const FVector Location = Hero->GetActorLocation() + AimDirection * SpawnForwardOffset;
	const FRotator Rotation = UKismetMathLibrary::FindLookAtRotation(Location, UZL_Xayah_ThrowAbility::TraceAimPoint(Hero));
	++BallsFired;
	StartSpawnTask(Location, Rotation);

	// After the last ball, its spawn report ends the ability.
	if (BallsFired < NumBalls && IsActive())
	{
		ArmConfirm();
	}
}

void UZL_Tempest_IAmNumber10::StartSpawnTask(const FVector& Location, const FRotator& Rotation)
{
	const TSubclassOf<AProjectile> ServerClass = ServerProjectileClass ? ServerProjectileClass : ProjectileClass;
	UZL_Task_SpawnPredictedProjectile* Task = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass, ServerClass, Location, Rotation);
	if (!Task)
	{
		OnSpawnReported();
		return;
	}
	Task->Success.AddDynamic(this, &UZL_Tempest_IAmNumber10::OnBallSpawned);
	Task->FailedToSpawn.AddDynamic(this, &UZL_Tempest_IAmNumber10::OnBallFailed);
	Task->ReadyForActivation();
}

void UZL_Tempest_IAmNumber10::OnBallSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_Tempest_StormBall* Ball = Cast<AZL_Tempest_StormBall>(SpawnedProjectile))
	{
		const int32 Level = GetAbilityLevel();
		Ball->InitStormBall(DirectDamage.GetValueAtLevel(Level), BurstDamage.GetValueAtLevel(Level),
			BurstRadius.GetValueAtLevel(Level) * GetAbilityRangeScale(), this);
	}
	OnSpawnReported();
}

void UZL_Tempest_IAmNumber10::OnBallFailed(AProjectile* SpawnedProjectile)
{
	OnSpawnReported();
}

void UZL_Tempest_IAmNumber10::OnSpawnReported()
{
	if (--PendingSpawns <= 0)
	{
		Stop();
	}
}

void UZL_Tempest_IAmNumber10::OnWindowFinished()
{
	Stop();
}

void UZL_Tempest_IAmNumber10::OnRepressed(float TimeWaited)
{
	Stop();
}

void UZL_Tempest_IAmNumber10::OnJumpPressed()
{
	if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		// Cancel only, no air jump.
		Hero->ConsumeJumpPress();
	}
	Stop();
}

void UZL_Tempest_IAmNumber10::Stop()
{
	if (IsActive() && !bEnding)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UZL_Tempest_IAmNumber10::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding)
	{
		return;
	}
	bEnding = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServerSafetyTimer);
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (bHolding)
	{
		bHolding = false;
		if (UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr)
		{
			Mover->QueueNextMode(FName("Falling"));
		}
	}
	if (bConfirmBound)
	{
		bConfirmBound = false;
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &UZL_Tempest_IAmNumber10::OnConfirmPressed);
		}
	}
	if (JumpHandle.IsValid())
	{
		if (Hero)
		{
			Hero->OnJumpInputPressed.Remove(JumpHandle);
		}
		JumpHandle.Reset();
	}
	if (WindowTask && WindowTask->IsActive())
	{
		WindowTask->OnFinished.RemoveDynamic(this, &UZL_Tempest_IAmNumber10::OnWindowFinished);
		WindowTask->EndTask();
	}
	WindowTask = nullptr;
	PendingSpawns = 0;

	// The client decides when it's over (its spawn data goes out before its end), so its end is replicated.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
