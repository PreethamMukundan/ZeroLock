// Copyright Preetham Mukundan (C) 2026


#include "GAS/TargetActors/ZL_PlacementClearance.h"

#include "Engine/World.h"

bool FZL_PlacementClearance::IsClear(const UWorld* World, const FTransform& Placement, const FBox& Footprint, const AActor* IgnoredActor) const
{
	if (!bEnabled)
	{
		return true;
	}
	if (!World || !Footprint.IsValid)
	{
		return false;
	}

	const FVector Forward = Placement.GetUnitAxis(EAxis::X);
	const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZL_PlacementClearance), false, IgnoredActor);

	// 1. Nothing inside the footprint or the space in front of it (floor, ceiling, pillars, other placed things).
	const FBox Space(FVector(SurfaceInset, Footprint.Min.Y + 1.0f, Footprint.Min.Z + FloorInset),
		FVector(FMath::Max(Footprint.Max.X, FrontClearance), Footprint.Max.Y - 1.0f, Footprint.Max.Z - 1.0f));
	if (Space.IsValid && World->OverlapAnyTestByObjectType(Placement.TransformPositionNoScale(Space.GetCenter()), Placement.GetRotation(),
		StaticObjects, FCollisionShape::MakeBox(Space.GetExtent()), Params))
	{
		return false;
	}

	// 2. Flat surface behind the whole footprint (its corners, edges and middle).
	if (bRequireFlushSurface)
	{
		const float Ys[] = { Footprint.Min.Y + 5.0f, Footprint.GetCenter().Y, Footprint.Max.Y - 5.0f };
		const float Zs[] = { Footprint.Min.Z + 20.0f, Footprint.GetCenter().Z, Footprint.Max.Z - 10.0f };
		for (const float Y : Ys)
		{
			for (const float Z : Zs)
			{
				FHitResult SurfaceHit;
				const FVector Start = Placement.TransformPositionNoScale(FVector(10.0f, Y, Z));
				const FVector End = Placement.TransformPositionNoScale(FVector(-MaxSurfaceGap, Y, Z));
				if (!World->LineTraceSingleByObjectType(SurfaceHit, Start, End, StaticObjects, Params)
					|| FVector::DotProduct(SurfaceHit.ImpactNormal, Forward) < 0.9f)
				{
					return false;
				}
			}
		}
	}

	// 3. Ground under both sides of the bottom.
	if (bRequireGround)
	{
		const float X = FMath::Max(Footprint.GetCenter().X, 1.0f);
		for (const float Y : { Footprint.Min.Y + 5.0f, Footprint.Max.Y - 5.0f })
		{
			FHitResult GroundHit;
			const FVector Start = Placement.TransformPositionNoScale(FVector(X, Y, Footprint.Min.Z + MaxGroundStep));
			const FVector End = Placement.TransformPositionNoScale(FVector(X, Y, Footprint.Min.Z - MaxGroundStep));
			if (!World->LineTraceSingleByObjectType(GroundHit, Start, End, StaticObjects, Params))
			{
				return false;
			}
		}
	}

	return true;
}
