// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseSurfaceTargetAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_Tempest_SettingAPick.generated.h"

class AZeroLockCharacter;

/**
 * Tempest's Setting a Pick. Aim at the ground (UZL_BaseSurfaceTargetAbility, Ground mode, a disc of PullRadius on the
 * spot), confirm: after ImpactDelay a gust there (server, on the hero's timers so the ability can end at once). Enemies
 * within PullRadius (no line of sight) take SpiritDamage, are pulled to the centre (PullDuration, PullTo victim event)
 * and slowed by SlowPercent for SlowDuration. Sets up Alley-oop.
 */
UCLASS()
class ZEROLOCK_API UZL_Tempest_SettingAPick : public UZL_BaseSurfaceTargetAbility
{
	GENERATED_BODY()

public:
	UZL_Tempest_SettingAPick();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Setting a Pick")
	float ImpactDelay = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Setting a Pick")
	FScalableFloat PullRadius = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Setting a Pick")
	FScalableFloat SpiritDamage = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Setting a Pick")
	float PullDuration = 0.3f;

	/** 30 = 30% slower. */
	UPROPERTY(EditDefaultsOnly, Category = "Setting a Pick")
	FScalableFloat SlowPercent = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Setting a Pick")
	FScalableFloat SlowDuration = 1.5f;

	/** The gust (server). Radius = PullRadius. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue BurstCue;

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDrawDebug = false;

	virtual float GetPreviewDiscRadius() const override;
	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data) override;

private:
	void Gust(const FVector& Location);
};
