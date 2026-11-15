// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseDashAbility.h"
#include "HeroKit/ZL_KitTypes.h"
#include "ZL_Kit_DashAbility.generated.h"

/**
 * Hero kit: UZL_BaseDashAbility (distance, duration, direction, hit sweep) with payloads: HitPayload on each enemy the
 * dash passes (instead of DashDamage), SelfPayload on the caster at the start (stealth, barrier, resist).
 */
UCLASS()
class ZEROLOCK_API UZL_Kit_DashAbility : public UZL_BaseDashAbility
{
	GENERATED_BODY()

public:
	UZL_Kit_DashAbility();

protected:
	virtual void OnDashStart_Implementation(const FVector& Direction) override;
	virtual void OnDashHit_Implementation(AZeroLockCharacter* Villan, const FHitResult& Hit) override;
	virtual FVector GetDashDirection_Implementation() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FScalableFloat PowerScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload HitPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload SelfPayload;

	/** Cue on the caster at the start (Duration about DashDuration for a trail loop). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|FX")
	FZL_KitCue DashCue;

	/** Dash flat along the aim instead of along the camera pitch. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bFlatDash = true;
};
