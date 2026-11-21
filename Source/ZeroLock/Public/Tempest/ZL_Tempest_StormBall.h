// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Tempest/ZL_Tempest_WindBall.h"
#include "ZL_Tempest_StormBall.generated.h"

class AZL_Tempest_Vortex;

/**
 * The big wind ball of Tempest's ultimate (I Am Number 10, "Eye of the Storm"). Doesn't steer; flies through players.
 * Each enemy it passes takes DirectDamage (HitHeroCue) and the ball bursts there; it also bursts where it stops (wall,
 * floor, end of life). A burst: BurstDamage to enemies in BurstRadius, pulls them to the burst point (PullDuration,
 * PullTo victim event) and leaves a wind dome (VortexClass) there. Each copy remembers who it passed, so the fake and the
 * server agree; damage is the server's.
 */
UCLASS()
class ZEROLOCK_API AZL_Tempest_StormBall : public AZL_Tempest_WindBall
{
	GENERATED_BODY()

public:
	AZL_Tempest_StormBall(const FObjectInitializer& ObjectInitializer);

	/** Server: the ability arms its copy after the spawn. */
	void InitStormBall(float InDirectDamage, float InBurstDamage, float InBurstRadius, const UObject* InDamageSource);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Storm Ball")
	float DirectDamage = 80.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Storm Ball")
	float PullDuration = 0.3f;

	/** Dome left at each burst. Empty = the native class (no material). */
	UPROPERTY(EditDefaultsOnly, Category = "Storm Ball")
	TSubclassOf<AZL_Tempest_Vortex> VortexClass;

	/** On each enemy the ball passes (server). */
	UPROPERTY(EditDefaultsOnly, Category = "Storm Ball")
	FZL_KitCue HitHeroCue;

	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	virtual void Burst(const FVector& Location) override;

private:
	TSet<TWeakObjectPtr<AActor>> PassedThrough;
};
