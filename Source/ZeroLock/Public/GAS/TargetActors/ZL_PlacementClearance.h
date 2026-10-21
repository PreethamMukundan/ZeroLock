// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ZL_PlacementClearance.generated.h"

class AActor;
class UWorld;

/**
 * Optional "doesn't clip" rules for placing something on a surface (Doorway's doors, AZL_GATA_SurfaceTrace). Works in
 * placement space: X out of the surface (the surface at X = 0), Z up, against a Footprint box (what gets placed).
 * Everything is WorldStatic only. Off (bEnabled false) = anything goes, the placement may clip.
 */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_PlacementClearance
{
	GENERATED_BODY()

	/** Off = no checks at all. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance)
	bool bEnabled = false;

	/** The footprint, extended out of the surface to at least this depth (room to step out), must be empty. cm. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float FrontClearance = 0.0f;

	/** The empty-space box starts this far off the surface and the footprint's bottom, so resting on them isn't a hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float SurfaceInset = 2.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance, meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float FloorInset = 8.0f;

	/** The surface must be flat behind the whole footprint (3x3 traces): no hanging over corners, edges or openings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance, meta = (EditCondition = "bEnabled"))
	bool bRequireFlushSurface = false;

	/** Flush: how far behind the surface plane each trace may find it. cm. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance, meta = (EditCondition = "bEnabled && bRequireFlushSurface", ClampMin = "1.0"))
	float MaxSurfaceGap = 10.0f;

	/** Ground under both bottom side edges of the footprint (not hanging off a step or ledge). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance, meta = (EditCondition = "bEnabled"))
	bool bRequireGround = false;

	/** Ground: how far above or below the footprint's bottom the ground may be. cm. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Clearance, meta = (EditCondition = "bEnabled && bRequireGround", ClampMin = "1.0"))
	float MaxGroundStep = 15.0f;

	/** True if disabled, or the footprint at Placement passes every enabled rule. IgnoredActor: usually the hero. */
	bool IsClear(const UWorld* World, const FTransform& Placement, const FBox& Footprint, const AActor* IgnoredActor) const;
};
