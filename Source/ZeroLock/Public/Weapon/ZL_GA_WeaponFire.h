// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_GA_WeaponFire.generated.h"

class AProjectile;
class AZeroLockCharacter;

UENUM(BlueprintType)
enum class EZLWeaponFireMode : uint8
{
	/** One projectile per activation (assault rifle). */
	Single,
	/** BurstCount projectiles per activation, BurstInterval apart. Each shot re-aims at the crosshair. */
	Burst,
	/** PelletCount projectiles at once, randomly spread inside a cone around the crosshair. */
	Shotgun
};

/**
 * Primary weapon fire. Spawns predicted projectiles aimed at whatever is under the screen-center crosshair.
 * Fire rate and ammo are driven by the character (FireRate attribute re-activates this ability on a timer); this
 * ability only decides what a single trigger pull fires.
 */
UCLASS()
class ZEROLOCK_API UZL_GA_WeaponFire : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_GA_WeaponFire();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Fake (client predicted) projectile class. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile")
	TSubclassOf<AProjectile> ProjectileClass;

	/** Authoritative server projectile class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile")
	TSubclassOf<AProjectile> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	EZLWeaponFireMode FireMode = EZLWeaponFireMode::Single;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Burst", Meta = (EditCondition = "FireMode == EZLWeaponFireMode::Burst", EditConditionHides, ClampMin = "1"))
	int32 BurstCount = 3;

	/** Seconds between shots in a burst. Keep BurstCount * BurstInterval below the FireRate attribute. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Burst", Meta = (EditCondition = "FireMode == EZLWeaponFireMode::Burst", EditConditionHides, ClampMin = "0.01"))
	float BurstInterval = 0.07f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Shotgun", Meta = (EditCondition = "FireMode == EZLWeaponFireMode::Shotgun", EditConditionHides, ClampMin = "1"))
	int32 PelletCount = 8;

	/** Half-angle of the pellet cone, in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Shotgun", Meta = (EditCondition = "FireMode == EZLWeaponFireMode::Shotgun", EditConditionHides, ClampMin = "0.0"))
	float SpreadAngle = 6.0f;

	/** Random spread applied to Single and Burst shots, in degrees. 0 = perfectly accurate. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", Meta = (ClampMin = "0.0"))
	float BulletSpreadAngle = 0.0f;

	/** Socket on the character mesh to fire from. Falls back to the actor location + SpawnForwardOffset if missing. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim")
	FName MuzzleSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim")
	float SpawnForwardOffset = 100.0f;

	/** How far the crosshair trace reaches. Shots that hit nothing aim at this distance. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim")
	float MaxAimDistance = 20000.0f;

	/** Projectiles still waiting to spawn (or, on a remote server, waiting on the client's spawn data). */
	int32 PendingSpawns = 0;

	/** Burst shots fired so far this activation (locally controlled only). */
	int32 BurstShotsFired = 0;

	int32 GetProjectilesPerActivation() const;

	/** Fires one burst shot and schedules the next. */
	void FireBurstShot();

	/** Fires a single projectile toward the crosshair, offset by up to SpreadDegrees. */
	void FireProjectile(float SpreadDegrees);

	void StartSpawnTask(const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** Where projectiles leave the character, and the world point under the crosshair. */
	void GetAimData(const AZeroLockCharacter* Hero, FVector& OutSpawnLocation, FVector& OutAimDirection) const;

	void OnSpawnFinished();

	UFUNCTION()
	void OnBurstIntervalFinished();

	UFUNCTION()
	void OnProjectileSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile);
};
