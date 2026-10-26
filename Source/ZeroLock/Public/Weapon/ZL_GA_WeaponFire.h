// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_GA_WeaponFire.generated.h"

class AProjectile;
class AZeroLockCharacter;
class UAnimMontage;

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

	/** No shooting while Viscous is a Goo Ball (Zerolock.Status.GooBall). */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** With bConsumeAmmo: needs at least one CurrentAmmo. */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	/** With bConsumeAmmo: UZL_GE_AmmoCost for one ammo per shot (a burst costs BurstCount, pellets cost nothing extra),
	 * capped at the ammo left. Predicted with the activation. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** Spend CurrentAmmo per shot. An empty clip auto-reloads (UBaseCharAttributeSet::PostAttributeChange). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	bool bConsumeAmmo = true;

	/** Takes the projectile class from the hero's WeaponStats when set, and FireMode, BurstCount/Interval and
	 * PelletCount/SpreadAngle when it has bOverrideFireMode. Runs before the commit, on the client and the server alike. */
	void ApplyHeroWeaponStats(const AZeroLockCharacter* Hero);

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

	/** Socket on the character mesh to fire from when the hero's WeaponStats.MuzzleSocket is None. Falls back to the
	 * actor location + SpawnForwardOffset if the mesh has neither. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim")
	FName MuzzleSocketName = NAME_None;

	/** Played once per trigger pull (predicted, replicated to other players). Empty = the hero's DeadlockAnimSet->ShootMontage. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimMontage> ShootMontage;

	void PlayShootMontage(const AZeroLockCharacter* Hero, const FGameplayAbilityActivationInfo& ActivationInfo);

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
