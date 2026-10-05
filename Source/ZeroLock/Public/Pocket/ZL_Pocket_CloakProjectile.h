// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_Pocket_CloakProjectile.generated.h"

class AZeroLockCharacter;

/**
 * Flying Cloak projectile. Collision only blocks WorldStatic (and slides along it), and the hitbox only overlaps heroes.
 * Enemies overlapped by the hitbox take spirit damage once; the projectile keeps flying through them.
 */
UCLASS()
class ZEROLOCK_API AZL_Pocket_CloakProjectile : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Pocket_CloakProjectile(const FObjectInitializer& ObjectInitializer);

	void SetSpiritDamage(float InDamage) { SpiritDamage = InDamage; }

protected:
	/** Fallback damage, overwritten by the owning ability once the projectile is spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	float SpiritDamage = 50.0f;

	/** Enemies already damaged by this projectile, so each one is only hit once. */
	TSet<TWeakObjectPtr<AZeroLockCharacter>> DamagedVillans;

	/** If true, the projectile keeps its full speed while sliding along a wall instead of losing the speed it had into the wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Slide")
	bool bPreserveSpeedOnSlide = true;

	/** Hits more head-on than this (|cos| between travel direction and wall normal) stop the projectile instead of sliding. 1 = never stop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Slide", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxSlideImpactDot = 0.95f;

	/** Hitting a wall strips the velocity into it, so the projectile slides along the surface. */
	virtual void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity) override;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	/** Hitting a wall only stops the projectile in place. It never detonates; the owning ability destroys it. */
	virtual void OnStop(const FHitResult& Hit) override;
};
