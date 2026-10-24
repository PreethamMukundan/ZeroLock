// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_Splatter.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Viscous/ZL_Projectile_Viscous_Splatter.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Viscous_Splatter::UZL_Viscous_Splatter()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ProjectileClass = AZL_Projectile_Viscous_Splatter::StaticClass();
}

void UZL_Viscous_Splatter::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	SpawnTask->Success.AddDynamic(this, &UZL_Viscous_Splatter::OnProjectileSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_Viscous_Splatter::OnProjectileFailedToSpawn);
	SpawnTask->ReadyForActivation();
}

void UZL_Viscous_Splatter::GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const
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

void UZL_Viscous_Splatter::OnProjectileSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_Projectile_Viscous_Splatter* Ball = Cast<AZL_Projectile_Viscous_Splatter>(SpawnedProjectile))
	{
		const int32 Level = GetAbilityLevel();
		// Every copy bounces the same number of times (replicated to the other clients' copies).
		Ball->SetGooBounces(FMath::FloorToInt32(MaxBounces.GetValueAtLevel(Level)));
		if (!Ball->bIsFakeProjectile)
		{
			Ball->ArmSplatter(ImpactDamage.GetValueAtLevel(Level), ImpactRadius.GetValueAtLevel(Level), PuddleRadius.GetValueAtLevel(Level),
				PuddleDuration.GetValueAtLevel(Level), SlowMultiplier.GetValueAtLevel(Level));
		}
	}

	// The ball outlives the ability.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Viscous_Splatter::OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}
