// Copyright Preetham Mukundan (C) 2026


#include "Debug/ZL_DebugDraw.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	/** World, plus every other PIE world when World is a PIE world. */
	template <typename DrawFn>
	void ForEachDebugWorld(const UWorld* World, DrawFn&& Draw)
	{
#if ENABLE_DRAW_DEBUG
		if (!World)
		{
			return;
		}
		Draw(World);

		if (World->WorldType != EWorldType::PIE || !GEngine)
		{
			return;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			const UWorld* Other = Context.World();
			if (Other && Other != World && Context.WorldType == EWorldType::PIE)
			{
				Draw(Other);
			}
		}
#endif
	}
}

void ZLDebugDraw::Sphere(const UWorld* World, const FVector& Center, float Radius, const FColor& Color, float LifeTime, int32 Segments)
{
#if ENABLE_DRAW_DEBUG
	ForEachDebugWorld(World, [&](const UWorld* W)
	{
		DrawDebugSphere(W, Center, Radius, Segments, Color, false, LifeTime);
	});
#endif
}

void ZLDebugDraw::Capsule(const UWorld* World, const FVector& Center, float HalfHeight, float Radius, const FColor& Color, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	ForEachDebugWorld(World, [&](const UWorld* W)
	{
		DrawDebugCapsule(W, Center, HalfHeight, Radius, FQuat::Identity, Color, false, LifeTime);
	});
#endif
}

void ZLDebugDraw::Cone(const UWorld* World, const FVector& Origin, const FVector& Direction, float Length, float AngleWidthRad, float AngleHeightRad,
	const FColor& Color, float LifeTime, int32 NumSides)
{
#if ENABLE_DRAW_DEBUG
	ForEachDebugWorld(World, [&](const UWorld* W)
	{
		DrawDebugCone(W, Origin, Direction, Length, AngleWidthRad, AngleHeightRad, NumSides, Color, false, LifeTime);
	});
#endif
}
