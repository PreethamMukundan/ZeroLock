// Copyright Preetham Mukundan (C) 2026


#include "Weapon/ZL_GA_WeaponFire.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Weapon/Projectile.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_GA_WeaponFire::UZL_GA_WeaponFire()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

	FGameplayTag ReloadTag = FGameplayTag::RequestGameplayTag(FName("ZeroLock.Weapon.Reloading"), false);
	if (ReloadTag.IsValid())
	{
		ActivationBlockedTags.AddTag(ReloadTag);
	}
}

void UZL_GA_WeaponFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	const int32 NumProjectiles = GetProjectilesPerActivation();
	PendingSpawns = NumProjectiles;
	BurstShotsFired = 0;

	if (!IsLocallyControlled())
	{
		// Remote server: every projectile is spawned from the client's spawn data. Each task consumes one projectile's
		// data, so the server never has to line up its own burst timer with the client's.
		for (int32 i = 0; i < NumProjectiles && IsActive(); ++i)
		{
			StartSpawnTask(Hero->GetActorLocation(), Hero->GetActorRotation());
		}
		return;
	}

	switch (FireMode)
	{
	case EZLWeaponFireMode::Burst:
		FireBurstShot();
		break;

	case EZLWeaponFireMode::Shotgun:
		for (int32 i = 0; i < NumProjectiles && IsActive(); ++i)
		{
			FireProjectile(SpreadAngle);
		}
		break;

	default:
		FireProjectile(BulletSpreadAngle);
		break;
	}
}

int32 UZL_GA_WeaponFire::GetProjectilesPerActivation() const
{
	switch (FireMode)
	{
	case EZLWeaponFireMode::Burst:
		return FMath::Max(1, BurstCount);
	case EZLWeaponFireMode::Shotgun:
		return FMath::Max(1, PelletCount);
	default:
		return 1;
	}
}

void UZL_GA_WeaponFire::FireBurstShot()
{
	FireProjectile(BulletSpreadAngle);

	if (!IsActive() || ++BurstShotsFired >= BurstCount)
	{
		return;
	}

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, BurstInterval);
	DelayTask->OnFinish.AddDynamic(this, &UZL_GA_WeaponFire::OnBurstIntervalFinished);
	DelayTask->ReadyForActivation();
}

void UZL_GA_WeaponFire::OnBurstIntervalFinished()
{
	FireBurstShot();
}

void UZL_GA_WeaponFire::FireProjectile(float SpreadDegrees)
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Hero))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	FVector SpawnLocation;
	FVector AimDirection;
	GetAimData(Hero, SpawnLocation, AimDirection);

	if (SpreadDegrees > 0.0f)
	{
		AimDirection = FMath::VRandCone(AimDirection, FMath::DegreesToRadians(SpreadDegrees));
	}

	StartSpawnTask(SpawnLocation, AimDirection.Rotation());
}

void UZL_GA_WeaponFire::StartSpawnTask(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	TSubclassOf<AProjectile> ServerClass = ServerProjectileClass ? ServerProjectileClass : ProjectileClass;
	UZL_Task_SpawnPredictedProjectile* SpawnTask = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass, ServerClass, SpawnLocation, SpawnRotation);
	if (!SpawnTask)
	{
		OnProjectileFailedToSpawn(nullptr);
		return;
	}
	SpawnTask->Success.AddDynamic(this, &UZL_GA_WeaponFire::OnProjectileSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_GA_WeaponFire::OnProjectileFailedToSpawn);
	SpawnTask->ReadyForActivation();
}

void UZL_GA_WeaponFire::GetAimData(const AZeroLockCharacter* Hero, FVector& OutSpawnLocation, FVector& OutAimDirection) const
{
	const UCameraComponent* Camera = Hero->GetFollowCamera();
	const FVector CameraLocation = Camera ? Camera->GetComponentLocation() : Hero->GetActorLocation();
	const FVector CameraDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : Hero->GetActorForwardVector();

	// Fire from the muzzle if the mesh has one, otherwise from just in front of the character.
	OutSpawnLocation = Hero->GetActorLocation() + CameraDirection * SpawnForwardOffset;
	if (MuzzleSocketName != NAME_None)
	{
		const USkeletalMeshComponent* Mesh = Hero->FindComponentByClass<USkeletalMeshComponent>();
		if (Mesh && Mesh->DoesSocketExist(MuzzleSocketName))
		{
			OutSpawnLocation = Mesh->GetSocketLocation(MuzzleSocketName);
		}
	}

	/* Trace straight out of the screen center. Start level with the character so nothing between the camera and the
	 * character (walls behind us, the boom) can be "aimed at". Trace by object type: the Hero capsule ignores the
	 * Visibility and Camera channels, and WorldDynamic is skipped so in-flight projectiles don't catch the trace. */
	const FVector TraceStart = CameraLocation + CameraDirection * FMath::Max(0.0f, FVector::DotProduct(Hero->GetActorLocation() - CameraLocation, CameraDirection));
	const FVector TraceEnd = TraceStart + CameraDirection * MaxAimDistance;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // Hero

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponAimTrace), true, Hero);

	FHitResult Hit;
	const FVector AimPoint = GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, QueryParams) ? Hit.ImpactPoint : TraceEnd;

	// Converge on the crosshair point. If it's behind or right on top of the muzzle (hugging a wall), just fire straight.
	const FVector ToAimPoint = AimPoint - OutSpawnLocation;
	OutAimDirection = (FVector::DotProduct(ToAimPoint, CameraDirection) > 10.0f) ? ToAimPoint.GetSafeNormal() : CameraDirection;
}

void UZL_GA_WeaponFire::OnProjectileSpawned(AProjectile* SpawnedProjectile)
{
	OnSpawnFinished();
}

void UZL_GA_WeaponFire::OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile)
{
	// One failed pellet/burst shot doesn't cancel the rest.
	OnSpawnFinished();
}

void UZL_GA_WeaponFire::OnSpawnFinished()
{
	// End once every projectile of this trigger pull has spawned, so delayed (high ping) spawns aren't cut off.
	if (--PendingSpawns <= 0 && IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UZL_GA_WeaponFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                   const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	PendingSpawns = 0;
	BurstShotsFired = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
