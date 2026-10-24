// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseSurfaceTargetAbility.h"
#include "ZL_Viscous_PuddlePunch.generated.h"

/**
 * Viscous's Puddle Punch. Aim at any surface (UZL_BaseSurfaceTargetAbility), confirm: a fist comes out of it along the
 * surface normal. Every hero in the box (PunchRange out of the surface, PunchWidth wide) is knocked along the normal
 * at KnockbackSpeed for KnockbackDuration: enemies, allies and Viscous himself. Enemies also take PunchDamage (melee).
 * Enemies that parry (ZeroLock.Melee.Parry) or are Untouchable take neither, and Viscous isn't stunned for it.
 * Others are moved through the Forced victim event (server); Viscous's own launch is predicted on his machine.
 */
UCLASS()
class ZEROLOCK_API UZL_Viscous_PuddlePunch : public UZL_BaseSurfaceTargetAbility
{
	GENERATED_BODY()

public:
	UZL_Viscous_PuddlePunch();

protected:
	/** How far the punch reaches out of the surface. */
	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch")
	FScalableFloat PunchRange = 400.0f;

	/** Width and height of the punch box, across the normal. */
	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch")
	FScalableFloat PunchWidth = 250.0f;

	/** Melee damage to each enemy hit. */
	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch")
	FScalableFloat PunchDamage = 75.0f;

	/** Wall punches: launch speed along the surface normal (cm/s), held for KnockbackDuration. */
	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch")
	FScalableFloat KnockbackSpeed = 1500.0f;

	/** Wall punches: upward speed added to the push (cm/s), so grounded targets leave the ground and slide less. */
	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch")
	FScalableFloat WallKnockbackLift = 450.0f;

	/** Floor and ceiling punches: launch speed along the normal (cm/s), straight up or down. */
	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch")
	FScalableFloat FloorKnockbackSpeed = 1000.0f;

	/** Normals within this many degrees of straight up or down count as floor / ceiling, the rest as wall. */
	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch", meta = (ClampMin = 0, ClampMax = 90))
	float FloorAngle = 45.0f;

	FVector GetKnockbackVelocity(const FVector& Normal, int32 Level) const;

	UPROPERTY(EditDefaultsOnly, Category = "PuddlePunch")
	float KnockbackDuration = 0.25f;

	/** The punch box itself (PunchRange × PunchWidth), so the preview shows what gets hit. PreviewSize is unused. */
	virtual FBox GetPreviewBox() const override;

	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data) override;
};
