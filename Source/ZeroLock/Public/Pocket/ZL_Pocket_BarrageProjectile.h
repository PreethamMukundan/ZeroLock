// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_Pocket_BarrageProjectile.generated.h"

class UGameplayEffect;

/**
 * Barrage projectile. Detonates on the first enemy or wall it hits, then sphere traces around the impact point and
 * deals spirit damage to every enemy inside. If at least one enemy was hit, the instigator gets one Barrage stack.
 */
UCLASS()
class ZEROLOCK_API AZL_Pocket_BarrageProjectile : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Pocket_BarrageProjectile(const FObjectInitializer& ObjectInitializer);

	/** Called by the owning ability once the projectile is spawned. Only matters on the server. */
	void InitBarrage(float InSpiritDamage, float InImpactRadius, TSubclassOf<UGameplayEffect> InStackEffect, int32 InEffectLevel);

protected:
	/** Fallback values, overwritten by the owning ability once the projectile is spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	float SpiritDamage = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	float ImpactRadius = 200.0f;

	/** Applied to the instigator when the impact hits at least one enemy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	TSubclassOf<UGameplayEffect> StackEffect;

	/** Draws the area of effect on impact. Yellow on clients; green (hit an enemy) or red (missed) on the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Debug")
	bool bDrawDebugImpact = true;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Debug", meta = (EditCondition = "bDrawDebugImpact"))
	float DebugDrawTime = 2.0f;

	int32 EffectLevel = 1;

	/** Guards against applying the impact twice (e.g. an overlap and a stop in the same frame). */
	bool bImpactApplied = false;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	virtual void OnStop(const FHitResult& Hit) override;

	/** Server only: runs once, right after the projectile detonates. */
	void ApplyImpact();
};
