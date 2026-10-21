// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_Projectile_Doorman_CallBell.generated.h"

class AZeroLockCharacter;
class USphereComponent;

/**
 * Doorman's Call Bell. A slow predicted projectile that flies through heroes and stops in place on WorldStatic.
 * - Impact: enemies the bell itself touches (hitbox sized to its mesh, or ImpactRadius) take ImpactDamage spirit damage, once each.
 * - Explosion: after FuseTime it deals ExplosionDamage spirit damage to enemies within ExplosionRadius, then is destroyed.
 * - Its owner can explode it early by shooting it with primary fire (ShootableComp, Hero object type). Other players'
 *   shots pass through.
 * Damage and fuse are server-only; the owning ability sets them with ArmBell.
 */
UCLASS()
class ZEROLOCK_API AZL_Projectile_Doorman_CallBell : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Projectile_Doorman_CallBell(const FObjectInitializer& ObjectInitializer);

	/** Server: sets the damage values and restarts the fuse. */
	void ArmBell(float InImpactDamage, float InExplosionDamage, float InFuseTime);

	/** Server: explosion damage, then detonates (FX, replication, destruction). */
	void Explode();

	virtual bool HandleHitByProjectile(AProjectile* OtherProjectile) override;

protected:
	/** Overlapped by the owner's primary-fire projectiles. Object type Hero, because weapon hitboxes only overlap Hero. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "CallBell")
	TObjectPtr<USphereComponent> ShootableComp;

	/** Radius of the hitbox that deals the impact damage. 0 = the bell's own size (largest static mesh on it). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell", meta = (ClampMin = "0.0"))
	float ImpactRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 350.0f;

	/** Radius the owner has to shoot to explode the bell early. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell", meta = (ClampMin = "1.0"))
	float ShootableRadius = 50.0f;

	/** Whether enemies walking into a bell that already stopped still take the impact damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell")
	bool bImpactDamageWhileStopped = false;

	/** Fallbacks, overwritten by the owning ability (ArmBell). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell|Damage")
	float ImpactDamage = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell|Damage")
	float ExplosionDamage = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell|Damage", meta = (ClampMin = "0.0"))
	float FuseTime = 3.0f;

	/** Draws the impact and explosion radii on the server. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CallBell|Debug")
	bool bDrawDebug = false;

	/** Enemies already hit by the impact, so each one is only hit once. */
	TSet<TWeakObjectPtr<AZeroLockCharacter>> DamagedVillans;

	FTimerHandle FuseTimer;
	bool bStopped = false;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	/** Hitting WorldStatic only stops the bell in place. It explodes on its fuse (or when shot). */
	virtual void OnStop(const FHitResult& Hit) override;

	bool IsAuthBell() const { return HasAuthority() && !bIsFakeProjectile; }

	/** ImpactRadius, or the half-size of the bell's largest static mesh (fallback 25 cm without one). */
	float GetImpactRadius() const;
	bool IsEnemy(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan) const;
};
