// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_SCombo_Projectile.generated.h"

class UStaticMeshComponent;

/** The ranged half of SCombo's slash (UZL_SCombo_Slash). Flies through teammates and detonates on the first enemy or wall; a direct hit deals spirit damage. */
UCLASS()
class ZEROLOCK_API AZL_SCombo_Projectile : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_SCombo_Projectile(const FObjectInitializer& ObjectInitializer);

	void SetSpiritDamage(float InDamage) { SpiritDamage = InDamage; }

protected:
	/** Fallback damage, overwritten by the owning ability once the projectile is spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	float SpiritDamage = 60.0f;

	/** Placeholder look until SCombo has real art. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> SlashMesh;

	/** Draws a sphere the size of the hitbox where the server's projectile ends: red on an enemy, yellow otherwise. On until the ability has a real effect. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	bool bDrawProjectileDebug = true;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	virtual void Destroyed() override;

private:
	bool bHitVillan = false;
};
