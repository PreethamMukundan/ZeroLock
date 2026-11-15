// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Debug shapes for server-side gameplay checks (hit sweeps, damage areas). Draws in World and, in a PIE session,
 * mirrors the shape into every other PIE world, so the client windows show what the server decided. Outside PIE
 * (standalone, packaged), only World gets it. Compiled out without ENABLE_DRAW_DEBUG.
 */
namespace ZLDebugDraw
{
	ZEROLOCK_API void Sphere(const UWorld* World, const FVector& Center, float Radius, const FColor& Color, float LifeTime, int32 Segments = 16);
	ZEROLOCK_API void Capsule(const UWorld* World, const FVector& Center, float HalfHeight, float Radius, const FColor& Color, float LifeTime);
	ZEROLOCK_API void Cone(const UWorld* World, const FVector& Origin, const FVector& Direction, float Length, float AngleWidthRad, float AngleHeightRad,
		const FColor& Color, float LifeTime, int32 NumSides = 16);
}
