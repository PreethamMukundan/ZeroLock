// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_Pocket_CloakProjectile.generated.h"

class AZeroLockCharacter;

/**
 * Flying Cloak projectile. Collision only blocks WorldStatic, and the hitbox only overlaps heroes.
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

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	/** Hitting a wall only stops the projectile in place. It never detonates; the owning ability destroys it. */
	virtual void OnStop(const FHitResult& Hit) override;
};
