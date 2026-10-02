// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_FlyingCloak.h"

#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Kismet/KismetMathLibrary.h"
#include "Mover/ZeroMoverComponent.h"
#include "Pocket/ZL_Pocket_CloakProjectile.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "Zero_BasePlayerController.h"

UZL_Pocket_FlyingCloak::UZL_Pocket_FlyingCloak()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UZL_Pocket_FlyingCloak::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                             const FGameplayAbilityActorInfo* ActorInfo,
                                             const FGameplayAbilityActivationInfo ActivationInfo,
                                             const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !ProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CloakProjectile.Reset();
	bHasProjectileLocation = false;

	FVector SpawnLocation;
	FRotator SpawnRotation;
	GetProjectileSpawnTransform(Hero, SpawnLocation, SpawnRotation);
	
	UZL_Task_SpawnPredictedProjectile* SpawnTask = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass, ServerProjectileClass, SpawnLocation, SpawnRotation);
	if (!SpawnTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SpawnTask->Success.AddDynamic(this, &UZL_Pocket_FlyingCloak::OnProjectileSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_Pocket_FlyingCloak::OnProjectileFailedToSpawn);
	SpawnTask->ReadyForActivation();

	// The spawn can fail synchronously and end the ability.
	if (!IsActive())
	{
		return;
	}

	WaitTimeTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, TeleportWindow.GetValueAtLevel(GetAbilityLevel()));
	WaitTimeTask->OnProgress.AddDynamic(this, &UZL_Pocket_FlyingCloak::UpdateProgressionTimer);
	WaitTimeTask->OnStarted.AddDynamic(this, &UZL_Pocket_FlyingCloak::StartProgressionTimer);
	WaitTimeTask->OnEnd.AddDynamic(this, &UZL_Pocket_FlyingCloak::StopProgressionTimer);
	WaitTimeTask->OnFinished.AddDynamic(this, &UZL_Pocket_FlyingCloak::OnTeleportWindowFinished);
	WaitTimeTask->ReadyForActivation();

	InputPressedTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	InputPressedTask->OnPress.AddDynamic(this, &UZL_Pocket_FlyingCloak::OnInputPressed);
	InputPressedTask->ReadyForActivation();
}

void UZL_Pocket_FlyingCloak::GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const
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

void UZL_Pocket_FlyingCloak::OnProjectileSpawned(AProjectile* SpawnedProjectile)
{
	if (!IsValid(SpawnedProjectile))
	{
		return;
	}

	CloakProjectile = SpawnedProjectile;
	SpawnedProjectile->OnDestroyed.AddDynamic(this, &UZL_Pocket_FlyingCloak::OnCloakProjectileDestroyed);

	if (AZL_Pocket_CloakProjectile* Cloak = Cast<AZL_Pocket_CloakProjectile>(SpawnedProjectile))
	{
		Cloak->SetSpiritDamage(SpiritDamage.GetValueAtLevel(GetAbilityLevel()));
	}
}

void UZL_Pocket_FlyingCloak::OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UZL_Pocket_FlyingCloak::OnCloakProjectileDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor)
	{
		LastProjectileLocation = DestroyedActor->GetActorLocation();
		bHasProjectileLocation = true;
	}
	CloakProjectile.Reset();
}

bool UZL_Pocket_FlyingCloak::GetTeleportLocation(FVector& OutLocation) const
{
	if (CloakProjectile.IsValid())
	{
		OutLocation = CloakProjectile->GetActorLocation();
		return true;
	}
	if (bHasProjectileLocation)
	{
		OutLocation = LastProjectileLocation;
		return true;
	}
	return false;
}

void UZL_Pocket_FlyingCloak::OnInputPressed(float TimeWaited)
{
	if (WaitTimeTask && WaitTimeTask->IsActive())
	{
		WaitTimeTask->OnFinished.RemoveDynamic(this, &UZL_Pocket_FlyingCloak::OnTeleportWindowFinished);
		WaitTimeTask->EndTask();
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	FVector TeleportLocation;
	if (IsValid(Hero) && GetTeleportLocation(TeleportLocation))
	{
		// Latched into the mover input stream so client and server teleport on the same frame,
		// using the client's projectile location (the server ignores its own request for remote players).
		if (UZeroMoverComponent* MoverComp = Hero->GetZeroMoverComponent())
		{
			MoverComp->RequestSafeTeleport(TeleportLocation);
		}
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Pocket_FlyingCloak::OnTeleportWindowFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Pocket_FlyingCloak::CleanupProjectile()
{
	AProjectile* Projectile = CloakProjectile.Get();
	CloakProjectile.Reset();
	bHasProjectileLocation = false;

	if (!IsValid(Projectile))
	{
		return;
	}

	Projectile->OnDestroyed.RemoveDynamic(this, &UZL_Pocket_FlyingCloak::OnCloakProjectileDestroyed);

	// A fake projectile that was never linked is still registered on the PC; remove it so the
	// authoritative projectile doesn't try to link to a destroyed actor.
	if (Projectile->bIsFakeProjectile)
	{
		if (AZero_BasePlayerController* ZeroPc = Cast<AZero_BasePlayerController>(GetActorInfo().PlayerController.Get()))
		{
			if (const uint32* Key = ZeroPc->FakeProjectiles.FindKey(Projectile))
			{
				ZeroPc->FakeProjectiles.Remove(*Key);
			}
		}
	}

	Projectile->Destroy();
}

void UZL_Pocket_FlyingCloak::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (WaitTimeTask && WaitTimeTask->IsActive())
	{
		WaitTimeTask->OnFinished.RemoveDynamic(this, &UZL_Pocket_FlyingCloak::OnTeleportWindowFinished);
		WaitTimeTask->EndTask();
	}
	WaitTimeTask = nullptr;

	if (InputPressedTask && InputPressedTask->IsActive())
	{
		InputPressedTask->OnPress.RemoveDynamic(this, &UZL_Pocket_FlyingCloak::OnInputPressed);
		InputPressedTask->EndTask();
	}
	InputPressedTask = nullptr;

	CleanupProjectile();

	CommitAbility(Handle, ActorInfo, ActivationInfo);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
