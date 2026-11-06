// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_ThrowAbility.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "Xayah/ZL_Xayah_BladeProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Xayah_ThrowAbility::UZL_Xayah_ThrowAbility()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ProjectileClass = AZL_Xayah_BladeProjectile::StaticClass();
}

void UZL_Xayah_ThrowAbility::ThrowFan()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Hero) || !ProjectileClass)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	PendingSpawns = BladeCount;

	// Remote server: each task takes its transform from one packet of the client's spawn data.
	if (!IsLocallyControlled())
	{
		for (int32 i = 0; i < BladeCount && IsActive(); ++i)
		{
			StartSpawnTask(Hero->GetActorLocation(), Hero->GetActorRotation());
		}
		return;
	}

	FVector SpawnLocation;
	FRotator AimRotation;
	GetAim(Hero, SpawnLocation, AimRotation);

	for (int32 i = 0; i < BladeCount && IsActive(); ++i)
	{
		StartSpawnTask(SpawnLocation, GetBladeRotation(AimRotation, i));
	}
}

FRotator UZL_Xayah_ThrowAbility::GetBladeRotation(const FRotator& AimRotation, int32 Index) const
{
	const float YawStep = BladeCount > 1 ? FanAngle / (BladeCount - 1) : 0.0f;
	const float FirstYaw = BladeCount > 1 ? -FanAngle * 0.5f : 0.0f;
	return AimRotation + FRotator(0.0f, FirstYaw + YawStep * Index, 0.0f);
}

FVector UZL_Xayah_ThrowAbility::TraceAimPoint(const AZeroLockCharacter* Hero)
{
	const UCameraComponent* Camera = Hero ? Hero->GetFollowCamera() : nullptr;
	if (!Camera)
	{
		return Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
	}

	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector TraceEnd = TraceStart + Camera->GetForwardVector().GetSafeNormal() * 10000.0f;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Hero);

	FHitResult Hit;
	return Hero->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) ? Hit.ImpactPoint : TraceEnd;
}

void UZL_Xayah_ThrowAbility::GetAim(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const
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
	OutRotation = UKismetMathLibrary::FindLookAtRotation(OutLocation, TraceAimPoint(Hero));
}

void UZL_Xayah_ThrowAbility::StartSpawnTask(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	const TSubclassOf<AProjectile> ClientClass = ProjectileClass;
	const TSubclassOf<AProjectile> ServerClass = ServerProjectileClass ? ServerProjectileClass : ProjectileClass;
	UZL_Task_SpawnPredictedProjectile* SpawnTask = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ClientClass, ServerClass, SpawnLocation, SpawnRotation);
	if (!SpawnTask)
	{
		OnBladeFailedToSpawn(nullptr);
		return;
	}
	SpawnTask->Success.AddDynamic(this, &UZL_Xayah_ThrowAbility::OnBladeSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_Xayah_ThrowAbility::OnBladeFailedToSpawn);
	SpawnTask->ReadyForActivation();
}

void UZL_Xayah_ThrowAbility::OnBladeSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_Xayah_BladeProjectile* Blade = Cast<AZL_Xayah_BladeProjectile>(SpawnedProjectile))
	{
		Blade->SetSpiritDamage(SpiritDamage.GetValueAtLevel(GetAbilityLevel()));
	}
	OnBladeFinished();
}

void UZL_Xayah_ThrowAbility::OnBladeFailedToSpawn(AProjectile* SpawnedProjectile)
{
	// One failed blade doesn't stop the rest of the fan.
	OnBladeFinished();
}

void UZL_Xayah_ThrowAbility::OnBladeFinished()
{
	if (--PendingSpawns <= 0 && IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}
