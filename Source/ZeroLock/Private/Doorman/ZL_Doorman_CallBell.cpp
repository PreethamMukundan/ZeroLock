// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_CallBell.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Camera/CameraComponent.h"
#include "Doorman/ZL_Projectile_Doorman_CallBell.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Doorman_CallBell::UZL_Doorman_CallBell()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	bIsChargedAbility = true;
	MaxChargesConfig = 2.0f;
	TimeBetweenCharges = 0.5f;
	Slot = EGameplayAbilitySlot::AbilitySlot1;
}

void UZL_Doorman_CallBell::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	if (ThrowMontage)
	{
		// Cosmetic: keeps playing after the ability ends.
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ThrowMontage, 1.0f, NAME_None, false);
		MontageTask->ReadyForActivation();
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;
	GetProjectileSpawnTransform(Hero, SpawnLocation, SpawnRotation);

	UZL_Task_SpawnPredictedProjectile* SpawnTask = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass,
		ServerProjectileClass ? ServerProjectileClass : ProjectileClass, SpawnLocation, SpawnRotation);
	if (!SpawnTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SpawnTask->Success.AddDynamic(this, &UZL_Doorman_CallBell::OnProjectileSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_Doorman_CallBell::OnProjectileFailedToSpawn);
	SpawnTask->ReadyForActivation();
}

void UZL_Doorman_CallBell::GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const
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

void UZL_Doorman_CallBell::OnProjectileSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_Projectile_Doorman_CallBell* Bell = Cast<AZL_Projectile_Doorman_CallBell>(SpawnedProjectile))
	{
		const float Level = GetAbilityLevel();
		const float Fuse = FuseTime.GetValueAtLevel(Level);
		if (Bell->bIsFakeProjectile)
		{
			// The server's explosion removes the fake; this only cleans up if the authoritative bell never links to it.
			Bell->SetLifeSpan(Fuse + 2.0f);
		}
		else
		{
			Bell->ArmBell(ImpactDamage.GetValueAtLevel(Level), ExplosionDamage.GetValueAtLevel(Level), Fuse);
		}
	}

	// The bell outlives the ability.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Doorman_CallBell::OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}
