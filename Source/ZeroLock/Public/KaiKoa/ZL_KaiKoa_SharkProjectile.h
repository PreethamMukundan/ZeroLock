// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "HeroKit/ZL_KitCue.h"
#include "Weapon/Projectile.h"
#include "ZL_KaiKoa_SharkProjectile.generated.h"

class AZeroLockCharacter;

/**
 * One shark of Kai Koa's Shark Shark Shark. Flies straight and swims through heroes: every enemy it touches takes
 * SpiritDamage once (server copy only) and gets HitHeroCue. A wall or the end of its life detonates it (HitWorldCue on
 * every copy, set on the BP). Looks: TrailCue on the BP; there is no mesh.
 */
UCLASS()
class ZEROLOCK_API AZL_KaiKoa_SharkProjectile : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_KaiKoa_SharkProjectile(const FObjectInitializer& ObjectInitializer);

	/** Server: the ability arms its copy after the spawn. */
	void InitShark(float InSpiritDamage, const UObject* InDamageSource);

protected:
	/** Fallback until the ability arms the projectile. */
	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	float SpiritDamage = 45.0f;

	/** Played on each enemy the shark bites (server, replicated). */
	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	FZL_KitCue HitHeroCue;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

private:
	/** Each enemy is bitten once. */
	TSet<TWeakObjectPtr<AZeroLockCharacter>> BittenVillans;

	/** Telemetry groups the damage by the ability. */
	TWeakObjectPtr<const UObject> DamageSource;
};
