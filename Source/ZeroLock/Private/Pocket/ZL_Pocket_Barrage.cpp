// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_Barrage.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Kismet/KismetMathLibrary.h"
#include "Mover/ZeroMoverComponent.h"
#include "Pocket/ZL_Pocket_BarrageProjectile.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Pocket_Barrage::UZL_Pocket_Barrage()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UZL_Pocket_Barrage::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo,
                                         const FGameplayAbilityActivationInfo ActivationInfo,
                                         const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ShotsFired = 0;
	PendingServerSpawns = 0;
	bFinishWhenSpawnsDone = false;

	// A new barrage always starts from zero stacks.
	if (HasAuthority(&ActivationInfo))
	{
		ClearStacks();
	}

	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));

		// MovementLock blocks other abilities; trying to use one ends the barrage and activates it instead.
		AbilityFailedHandle = ASC->AbilityFailedCallbacks.AddUObject(this, &UZL_Pocket_Barrage::OnAbilityFailed);
	}

	// Float along the current momentum; the floating mode only lets move input steer it slightly.
	if (UZeroMoverComponent* MoverComp = Hero->GetZeroMoverComponent())
	{
		MoverComp->QueueNextMode(FloatingModeName);
	}
	bIsFloating = true;

	if (!IsLocallyControlled())
	{
		// Remote server: the projectiles are spawned from the client's spawn data. Every task listens for it and
		// each one consumes a single projectile, so the server never has to line its own timer up with the client's.
		PendingServerSpawns = NumProjectiles;
		for (int32 i = 0; i < NumProjectiles && IsActive(); ++i)
		{
			StartSpawnTask(Hero->GetActorLocation(), Hero->GetActorRotation());
		}

		if (IsActive())
		{
			StartFloatTimer();
			ListenForCancel(Hero);
		}
		return;
	}

	StartFloatTimer();
	ListenForCancel(Hero);
	FireProjectile();
}

void UZL_Pocket_Barrage::StartFloatTimer()
{
	float Duration = FloatDuration.GetValueAtLevel(GetAbilityLevel());
	if (bScaleFireIntervalToDuration)
	{
		// Spread the shots evenly: the first fires on activation, the last one interval before the float ends.
		CurrentFireInterval = Duration / NumProjectiles;
	}
	else
	{
		CurrentFireInterval = FireInterval;
		Duration = FMath::Max(Duration, FireInterval * (NumProjectiles - 1));
	}

	FloatTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, Duration);
	FloatTask->OnProgress.AddDynamic(this, &UZL_Pocket_Barrage::UpdateProgressionTimer);
	FloatTask->OnStarted.AddDynamic(this, &UZL_Pocket_Barrage::StartProgressionTimer);
	FloatTask->OnEnd.AddDynamic(this, &UZL_Pocket_Barrage::StopProgressionTimer);
	FloatTask->OnFinished.AddDynamic(this, &UZL_Pocket_Barrage::OnFloatFinished);
	FloatTask->ReadyForActivation();
}

void UZL_Pocket_Barrage::ListenForCancel(AZeroLockCharacter* Hero)
{
	if (!bCanCancelEarly)
	{
		return;
	}

	// Pressing the ability input again. Works on the client and (through input replication) the server.
	UAbilityTask_WaitInputPress* InputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	InputPressTask->OnPress.AddDynamic(this, &UZL_Pocket_Barrage::OnAbilityInputPressed);
	InputPressTask->ReadyForActivation();

	// Jump only exists as mover input, so only the owning client sees it. Its EndAbility is replicated to the server.
	if (IsLocallyControlled() && Hero)
	{
		JumpPressedHandle = Hero->OnJumpInputPressed.AddUObject(this, &UZL_Pocket_Barrage::OnJumpPressed);
	}
}

void UZL_Pocket_Barrage::OnJumpPressed()
{
	// Jump only cancels the barrage; swallow the press so it doesn't also trigger an (air) jump.
	if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		Hero->ConsumeJumpPress();
	}
	CancelBarrage();
}

void UZL_Pocket_Barrage::OnAbilityInputPressed(float TimeWaited)
{
	CancelBarrage();
}

void UZL_Pocket_Barrage::OnAbilityFailed(const UGameplayAbility* FailedAbility, const FGameplayTagContainer& FailureTags)
{
	const UBaseGameplayAbility* OtherAbility = Cast<UBaseGameplayAbility>(FailedAbility);
	if (!OtherAbility || OtherAbility->GetClass() == GetClass() || OtherAbility->AbilityInputID == EGASAbilityInputID::None
		|| InputsThatDontCancel.Contains(OtherAbility->AbilityInputID))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpec* OtherSpec = ASC ? ASC->FindAbilitySpecFromClass(OtherAbility->GetClass()) : nullptr;
	if (!OtherSpec)
	{
		return;
	}

	// Only step aside when the barrage is what's in the way: an ability on cooldown or without its cost stays failed.
	const FGameplayAbilitySpecHandle OtherHandle = OtherSpec->Handle;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!OtherAbility->CheckCooldown(OtherHandle, ActorInfo) || !OtherAbility->CheckCost(OtherHandle, ActorInfo))
	{
		return;
	}

	/* Only the predicting client gets here (a blocked activation never reaches the server). Our replicated EndAbility is
	 * sent before the retry's activation RPC, so the server ends its barrage first and then lets the new ability through. */
	CancelBarrage();
	ASC->TryActivateAbility(OtherHandle);
}

void UZL_Pocket_Barrage::CancelBarrage()
{
	if (IsActive())
	{
		// Cost and cooldown were committed on activation, so cancelling just ends the barrage right away.
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UZL_Pocket_Barrage::FireProjectile()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Hero))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;
	GetProjectileSpawnTransform(Hero, SpawnLocation, SpawnRotation);
	StartSpawnTask(SpawnLocation, SpawnRotation);

	if (!IsActive())
	{
		return;
	}

	// After the last shot, the float timer ends the ability.
	if (++ShotsFired >= NumProjectiles)
	{
		return;
	}

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CurrentFireInterval);
	DelayTask->OnFinish.AddDynamic(this, &UZL_Pocket_Barrage::OnFireIntervalFinished);
	DelayTask->ReadyForActivation();
}

void UZL_Pocket_Barrage::StartSpawnTask(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	TSubclassOf<AProjectile> ServerClass = ServerProjectileClass ? ServerProjectileClass : ProjectileClass;
	UZL_Task_SpawnPredictedProjectile* SpawnTask = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass, ServerClass, SpawnLocation, SpawnRotation);
	if (!SpawnTask)
	{
		OnProjectileFailedToSpawn(nullptr);
		return;
	}
	SpawnTask->Success.AddDynamic(this, &UZL_Pocket_Barrage::OnProjectileSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_Pocket_Barrage::OnProjectileFailedToSpawn);
	SpawnTask->ReadyForActivation();
}

void UZL_Pocket_Barrage::GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const
{
	const UCameraComponent* Camera = Hero->GetFollowCamera();
	const FVector AimDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : Hero->GetActorForwardVector();

	OutLocation = Hero->GetActorLocation() + AimDirection * SpawnForwardOffset;
	OutRotation = AimDirection.Rotation();

	if (!Camera)
	{
		return;
	}

	// Aim at whatever the camera is looking at, like the other projectile abilities.
	const FVector CamTraceStart = Camera->GetComponentLocation();
	const FVector CamTraceEnd = CamTraceStart + AimDirection * 10000.0f;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Hero);

	FHitResult Hit;
	const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, CamTraceStart, CamTraceEnd, ECC_Visibility, QueryParams) ? Hit.ImpactPoint : CamTraceEnd;
	OutRotation = UKismetMathLibrary::FindLookAtRotation(OutLocation, AimPoint);
}

void UZL_Pocket_Barrage::OnFireIntervalFinished()
{
	FireProjectile();
}

void UZL_Pocket_Barrage::OnFloatFinished()
{
	FinishBarrage();
}

void UZL_Pocket_Barrage::FinishBarrage()
{
	// The remote server keeps the ability alive until every projectile the client fired has been spawned.
	// (The client's replicated EndAbility always arrives after its spawn data, so this only matters if our timer runs out first.)
	if (PendingServerSpawns > 0)
	{
		bFinishWhenSpawnsDone = true;
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Pocket_Barrage::OnProjectileSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_Pocket_BarrageProjectile* Barrage = Cast<AZL_Pocket_BarrageProjectile>(SpawnedProjectile))
	{
		const float Level = GetAbilityLevel();
		Barrage->InitBarrage(SpiritDamage.GetValueAtLevel(Level), ImpactRadius.GetValueAtLevel(Level), StackEffect, GetAbilityLevel());	}

	if (PendingServerSpawns > 0)
	{
		--PendingServerSpawns;
		if (bFinishWhenSpawnsDone)
		{
			FinishBarrage();
		}
	}
}

void UZL_Pocket_Barrage::OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile)
{
	// A single failed projectile doesn't cancel the rest of the barrage.
	if (PendingServerSpawns > 0)
	{
		--PendingServerSpawns;
		if (bFinishWhenSpawnsDone)
		{
			FinishBarrage();
		}
	}
}

void UZL_Pocket_Barrage::ClearStacks() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StackEffect)
	{
		ASC->RemoveActiveGameplayEffectBySourceEffect(StackEffect, nullptr);
	}
}

void UZL_Pocket_Barrage::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (AbilityFailedHandle.IsValid())
	{
		if (ASC)
		{
			ASC->AbilityFailedCallbacks.Remove(AbilityFailedHandle);
		}
		AbilityFailedHandle.Reset();
	}

	if (bIsFloating)
	{
		bIsFloating = false;

		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
		}

		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
		if (Hero && Hero->GetZeroMoverComponent())
		{
			Hero->GetZeroMoverComponent()->QueueNextMode(FName("Falling"));
		}
	}

	if (JumpPressedHandle.IsValid())
	{
		if (AZeroLockCharacter* JumpHero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
		{
			JumpHero->OnJumpInputPressed.Remove(JumpPressedHandle);
		}
		JumpPressedHandle.Reset();
	}

	if (FloatTask && FloatTask->IsActive())
	{
		FloatTask->OnFinished.RemoveDynamic(this, &UZL_Pocket_Barrage::OnFloatFinished);
		FloatTask->EndTask();
	}
	FloatTask = nullptr;

	PendingServerSpawns = 0;
	bFinishWhenSpawnsDone = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
