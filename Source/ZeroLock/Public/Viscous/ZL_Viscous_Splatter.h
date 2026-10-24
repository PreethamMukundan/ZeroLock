// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Viscous_Splatter.generated.h"

class AProjectile;
class AZeroLockCharacter;
class AZL_Projectile_Viscous_Splatter;
class UAnimMontage;

/**
 * Viscous's Splatter. Commits and throws one predicted AZL_Projectile_Viscous_Splatter (camera-aimed, like Call Bell),
 * then ends; the ball lives on. It passes through heroes and bounces MaxBounces times (scales with level); every
 * impact deals ImpactDamage (spirit) to enemies within ImpactRadius and leaves a goo puddle on floors that slows
 * enemies inside it (SlowMultiplier) for PuddleDuration.
 */
UCLASS()
class ZEROLOCK_API UZL_Viscous_Splatter : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Viscous_Splatter();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Fake (client predicted) projectile class. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Projectile_Viscous_Splatter> ProjectileClass;

	/** Authoritative server projectile class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Projectile_Viscous_Splatter> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float SpawnForwardOffset = 100.0f;

	/** Bounces before the impact that ends the ball (rounded down). */
	UPROPERTY(EditDefaultsOnly, Category = "Splatter")
	FScalableFloat MaxBounces = 2.0f;

	/** Spirit damage to each enemy within ImpactRadius, at every impact. */
	UPROPERTY(EditDefaultsOnly, Category = "Splatter")
	FScalableFloat ImpactDamage = 70.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Splatter")
	FScalableFloat ImpactRadius = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Splatter|Puddle")
	FScalableFloat PuddleRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Splatter|Puddle")
	FScalableFloat PuddleDuration = 4.0f;

	/** Enemies in a puddle move at this fraction of their speed (0.6 = 40% slower). */
	UPROPERTY(EditDefaultsOnly, Category = "Splatter|Puddle")
	FScalableFloat SlowMultiplier = 0.6f;

	/** Optional throw montage (cosmetic, not waited on). */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ThrowMontage;

	UFUNCTION()
	void OnProjectileSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile);

	void GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const;
};
