// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Doorman_CallBell.generated.h"

class AProjectile;
class AZeroLockCharacter;
class AZL_Projectile_Doorman_CallBell;
class UAnimMontage;

/**
 * Doorman throws a Call Bell (predicted projectile). Charged ability (MaxChargesConfig charges). The bell lives on
 * after the ability ends: impact damage to enemies it passes, explosion after the fuse (or when Doorman shoots it).
 */
UCLASS()
class ZEROLOCK_API UZL_Doorman_CallBell : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Doorman_CallBell();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Fake (client predicted) projectile class. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Projectile_Doorman_CallBell> ProjectileClass;

	/** Authoritative server projectile class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Projectile_Doorman_CallBell> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float SpawnForwardOffset = 100.0f;

	/** Spirit damage to each enemy the flying bell passes through. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat ImpactDamage = 40.0f;

	/** Spirit damage to enemies in the bell's ExplosionRadius when it explodes. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat ExplosionDamage = 80.0f;

	/** Seconds from the throw until the bell explodes on its own. */
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	FScalableFloat FuseTime = 3.0f;

	/** Optional throw montage (cosmetic, not waited on). */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ThrowMontage;

	UFUNCTION()
	void OnProjectileSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile);

	void GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const;
};
