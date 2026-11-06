// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_Xayah_WeaponProjectile.generated.h"

class AZeroLockCharacter;
class AZL_Xayah_Feather;

/**
 * Xayah's primary fire bullet. Weapon damage as usual, but it flies through heroes (bPierceHeroes), hitting each enemy
 * once, and where it lands it spends a Clean Cut shot, if she has one, to leave a feather.
 */
UCLASS()
class ZEROLOCK_API AZL_Xayah_WeaponProjectile : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Xayah_WeaponProjectile(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Feather")
	TSubclassOf<AZL_Xayah_Feather> FeatherClass;

	/** Off: the bullet stops on the first hero it hits, like every other hero's. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Damage")
	bool bPierceHeroes = true;

	/** Enemies already hit by this bullet. */
	TSet<TWeakObjectPtr<AZeroLockCharacter>> DamagedVillans;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	virtual void OnStop(const FHitResult& Hit) override;

private:
	/** Server: call after anything that may have detonated this bullet; bWasDetonated = its state before that. A feather
	 * sticks into Surface if there is one (walls), and falls to the floor if not (end of its life, or a hit on a hero
	 * with bPierceHeroes off). */
	void DropFeatherOnDetonation(bool bWasDetonated, const FHitResult* Surface);
};
