// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_ProjectileAbility.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Mover/ZeroMoverComponent.h"
#include "TimerManager.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "Xayah/ZL_Xayah_ThrowAbility.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Kit_ProjectileAbility::UZL_Kit_ProjectileAbility()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ProjectileClass = AZL_Kit_Projectile::StaticClass();
}

void UZL_Kit_ProjectileAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEnding = false;
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); ASC && CastMontage)
	{
		ASC->PlayMontage(this, ActivationInfo, CastMontage, 1.0f);
	}
	if (HasAuthority(&ActivationInfo))
	{
		ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), SelfPayload, PowerScale.GetValueAtLevel(GetAbilityLevel()));
	}

	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), CastCue, Hero->GetActorLocation(), GetHeroAimDirection().Rotation());

	FTimerManager& Timers = GetWorld()->GetTimerManager();
	const bool bMoves = !FMath::IsNearlyZero(MoveForwardSpeed) || !FMath::IsNearlyZero(MoveUpSpeed);
	if (bMoves && IsLocallyControlled())
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			const FVector Forward = GetHeroAimDirection().GetSafeNormal2D();
			Mover->RequestSafeAbilityMove(Forward * MoveForwardSpeed + FVector::UpVector * MoveUpSpeed, FMath::Max(MoveDuration, 0.01f));
			if (bStopAfterMove)
			{
				Timers.SetTimer(MoveTimer, this, &UZL_Kit_ProjectileAbility::OnMoveFinished, FMath::Max(MoveDuration, 0.01f), false);
			}
		}
	}

	// Hang in the air (Death Slam style) once the leap is done, on every machine so the Mover agrees.
	bHolding = false;
	if (!HoldModeName.IsNone())
	{
		if (bMoves && MoveDuration > 0.0f)
		{
			Timers.SetTimer(HoldTimer, this, &UZL_Kit_ProjectileAbility::EnterHoldMode, MoveDuration, false);
		}
		else
		{
			EnterHoldMode();
		}
	}

	PendingSpawns = Count * VolleyCount;
	VolleysFired = 0;
	ArmedCount = 0;
	NextInterval = VolleyInterval;

	// Remote server: every task waits for one packet of the client's spawn data, so they all start now.
	if (!IsLocallyControlled())
	{
		for (int32 Index = 0; Index < Count * VolleyCount && IsActive(); ++Index)
		{
			StartSpawnTask(Hero->GetActorLocation(), Hero->GetActorRotation());
		}
		if (bFireOnConfirm && IsActive())
		{
			// The client ends it (replicated) after its last click; this only catches a client that never does.
			GetWorld()->GetTimerManager().SetTimer(ServerSafetyTimer, this, &UZL_Kit_ProjectileAbility::EndManual,
				FireDelay + ConfirmWindow + 3.0f, false);
		}
		return;
	}

	if (bFireOnConfirm)
	{
		StartManualFire(Hero);
		return;
	}

	if (FireDelay > 0.0f)
	{
		Timers.SetTimer(VolleyTimer, this, &UZL_Kit_ProjectileAbility::FireVolley, FireDelay, false);
	}
	else
	{
		FireVolley();
	}
}

void UZL_Kit_ProjectileAbility::GetAim(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const
{
	const UCameraComponent* Camera = Hero->GetFollowCamera();
	const FVector AimDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : Hero->GetActorForwardVector();
	OutLocation = Hero->GetActorLocation() + AimDirection * SpawnForwardOffset;
	OutRotation = Camera ? UKismetMathLibrary::FindLookAtRotation(OutLocation, UZL_Xayah_ThrowAbility::TraceAimPoint(Hero)) : AimDirection.Rotation();
	OutRotation.Pitch += PitchOffset;
}

void UZL_Kit_ProjectileAbility::FireVolley()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!IsActive() || !Hero)
	{
		return;
	}

	FVector Location;
	FRotator Aim;
	GetAim(Hero, Location, Aim);
	const float YawStep = Count > 1 ? FanAngle / (Count - 1) : 0.0f;
	const float FirstYaw = Count > 1 ? -FanAngle * 0.5f : 0.0f;
	for (int32 Index = 0; Index < Count && IsActive(); ++Index)
	{
		StartSpawnTask(Location, Aim + FRotator(0.0f, FirstYaw + YawStep * Index, 0.0f));
	}

	++VolleysFired;
	if (!bFireOnConfirm && VolleysFired < VolleyCount && IsActive())
	{
		GetWorld()->GetTimerManager().SetTimer(VolleyTimer, this, &UZL_Kit_ProjectileAbility::FireVolley, FMath::Max(NextInterval, 0.02f), false);
		NextInterval *= FMath::Max(VolleyIntervalScale, 0.05f);
	}
}

void UZL_Kit_ProjectileAbility::StartSpawnTask(const FVector& Location, const FRotator& Rotation)
{
	const TSubclassOf<AProjectile> ClientClass = ProjectileClass;
	const TSubclassOf<AProjectile> ServerClass = ServerProjectileClass ? ServerProjectileClass : ProjectileClass;
	UZL_Task_SpawnPredictedProjectile* Task = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ClientClass, ServerClass, Location, Rotation);
	if (!Task)
	{
		OnSpawnFinished();
		return;
	}
	Task->Success.AddDynamic(this, &UZL_Kit_ProjectileAbility::OnProjectileSpawned);
	Task->FailedToSpawn.AddDynamic(this, &UZL_Kit_ProjectileAbility::OnProjectileFailed);
	Task->ReadyForActivation();
}

FZL_KitProjectileHit UZL_Kit_ProjectileAbility::GetHitForVolley(int32 Volley) const
{
	FZL_KitProjectileHit Result = Hit;
	Result.AreaRadius += AreaRadiusPerVolley * Volley;
	return Result;
}

void UZL_Kit_ProjectileAbility::OnProjectileSpawned(AProjectile* Spawned)
{
	AZL_Kit_Projectile* Projectile = Cast<AZL_Kit_Projectile>(Spawned);
	if (Projectile && Projectile->HasAuthority() && !Projectile->bIsFakeProjectile)
	{
		const int32 Volley = Count > 0 ? ArmedCount / Count : 0;
		++ArmedCount;
		Projectile->Arm(GetHitForVolley(Volley), PowerScale.GetValueAtLevel(GetAbilityLevel()), GetAbilityRangeScale(), GetClass()->GetFName());
		OnProjectileArmed(Projectile);
	}
	OnSpawnFinished();
}

void UZL_Kit_ProjectileAbility::OnProjectileFailed(AProjectile* Spawned)
{
	OnSpawnFinished();
}

void UZL_Kit_ProjectileAbility::OnSpawnFinished()
{
	if (--PendingSpawns <= 0 && IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UZL_Kit_ProjectileAbility::OnMoveFinished()
{
	if (const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			Mover->RequestSafeStop();
		}
	}
}

void UZL_Kit_ProjectileAbility::EnterHoldMode()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
	if (!Mover || !IsActive() || bEnding)
	{
		return;
	}
	Mover->QueueNextMode(HoldModeName);
	bHolding = true;
}

void UZL_Kit_ProjectileAbility::StartManualFire(AZeroLockCharacter* Hero)
{
	WindowTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, FMath::Max(FireDelay + ConfirmWindow, 0.1f));
	WindowTask->OnProgress.AddDynamic(this, &UZL_Kit_ProjectileAbility::UpdateProgressionTimer);
	WindowTask->OnStarted.AddDynamic(this, &UZL_Kit_ProjectileAbility::StartProgressionTimer);
	WindowTask->OnEnd.AddDynamic(this, &UZL_Kit_ProjectileAbility::StopProgressionTimer);
	WindowTask->OnFinished.AddDynamic(this, &UZL_Kit_ProjectileAbility::OnConfirmWindowFinished);
	WindowTask->ReadyForActivation();

	if (bCancelOnRepress)
	{
		UAbilityTask_WaitInputPress* Repress = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
		Repress->OnPress.AddDynamic(this, &UZL_Kit_ProjectileAbility::OnRepressed);
		Repress->ReadyForActivation();
	}
	if (bCancelOnJump && Hero)
	{
		JumpHandle = Hero->OnJumpInputPressed.AddUObject(this, &UZL_Kit_ProjectileAbility::OnJumpPressed);
	}

	if (FireDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(VolleyTimer, this, &UZL_Kit_ProjectileAbility::ArmConfirm, FireDelay, false);
	}
	else
	{
		ArmConfirm();
	}
}

void UZL_Kit_ProjectileAbility::ArmConfirm()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !IsActive() || bConfirmBound)
	{
		return;
	}
	// The ASC clears its confirm callbacks after every confirm, so this is re-bound per shot.
	ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &UZL_Kit_ProjectileAbility::OnConfirmPressed);
	bConfirmBound = true;
}

void UZL_Kit_ProjectileAbility::OnConfirmPressed()
{
	bConfirmBound = false;
	if (!IsActive() || bEnding)
	{
		return;
	}
	FireVolley();
	// After the last volley, the spawn tasks end the ability when they report.
	if (VolleysFired < VolleyCount && IsActive())
	{
		ArmConfirm();
	}
}

void UZL_Kit_ProjectileAbility::OnConfirmWindowFinished()
{
	EndManual();
}

void UZL_Kit_ProjectileAbility::OnRepressed(float TimeWaited)
{
	EndManual();
}

void UZL_Kit_ProjectileAbility::OnJumpPressed()
{
	if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		Hero->ConsumeJumpPress(); // cancel only, no air jump
	}
	EndManual();
}

void UZL_Kit_ProjectileAbility::EndManual()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UZL_Kit_ProjectileAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding)
	{
		return;
	}
	bEnding = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VolleyTimer);
		World->GetTimerManager().ClearTimer(MoveTimer);
		World->GetTimerManager().ClearTimer(ServerSafetyTimer);
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	if (bHolding)
	{
		bHolding = false;
		if (const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
			{
				Mover->QueueNextMode(FName("Falling"));
			}
		}
	}
	if (bConfirmBound)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &UZL_Kit_ProjectileAbility::OnConfirmPressed);
		}
		bConfirmBound = false;
	}
	if (JumpHandle.IsValid())
	{
		if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
		{
			Hero->OnJumpInputPressed.Remove(JumpHandle);
		}
		JumpHandle.Reset();
	}
	if (WindowTask && WindowTask->IsActive())
	{
		WindowTask->OnFinished.RemoveDynamic(this, &UZL_Kit_ProjectileAbility::OnConfirmWindowFinished);
		WindowTask->EndTask();
	}
	WindowTask = nullptr;
	// The client finishes spawning first; its end must not cancel the server's tasks still waiting for spawn data.
	// Manual fire is the exception: the client decides when it's over (its spawn data is sent before the end).
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority() && !bFireOnConfirm)
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
