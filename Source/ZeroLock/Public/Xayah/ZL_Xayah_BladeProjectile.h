// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_Xayah_BladeProjectile.generated.h"

class AZeroLockCharacter;
class AZL_Xayah_Feather;
class UStaticMeshComponent;

/**
 * Xayah's thrown blade (Double Daggers, Featherstorm). Flies through enemies, dealing spirit damage once to each, and
 * drops a feather where it stops: a wall, or the end of its life (range = speed × InitialLifeSpan).
 */
UCLASS()
class ZEROLOCK_API AZL_Xayah_BladeProjectile : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Xayah_BladeProjectile(const FObjectInitializer& ObjectInitializer);

	void SetSpiritDamage(float InDamage) { SpiritDamage = InDamage; }

protected:
	/** Fallback damage, overwritten by the owning ability once the projectile is spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	float SpiritDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Feather")
	TSubclassOf<AZL_Xayah_Feather> FeatherClass;

	/** Placeholder look until Xayah has real art. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> BladeMesh;

	/** Enemies already damaged by this blade. */
	TSet<TWeakObjectPtr<AZeroLockCharacter>> DamagedVillans;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	virtual void OnStop(const FHitResult& Hit) override;
};
