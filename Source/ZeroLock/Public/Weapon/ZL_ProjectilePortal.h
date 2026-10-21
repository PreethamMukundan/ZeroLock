// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"
#include "ZL_ProjectilePortal.generated.h"

UINTERFACE(MinimalAPI)
class UZL_ProjectilePortal : public UInterface
{
	GENERATED_BODY()
};

/**
 * Actor whose blocking surface sends projectiles somewhere else (Doorman's Doorway). UZL_ProjectileMovementComponent
 * asks it on every blocking hit; returning true moves the projectile instead of bouncing/stopping it.
 */
class ZEROLOCK_API IZL_ProjectilePortal
{
	GENERATED_BODY()

public:
	/**
	 * Where a projectile that hit this portal (Hit.Location = its center at impact) with Velocity comes out. False =
	 * treat as a normal wall (also when Hit is on another part of the actor, e.g. a door frame). ProjectileRadius is
	 * its collision radius: the exit must leave it clear of the exit's own geometry, or the (non-bouncing) projectile
	 * starts penetrating and detonates there.
	 */
	virtual bool GetPortalExit(const FHitResult& Hit, const FVector& Velocity, float ProjectileRadius, FVector& OutLocation, FVector& OutVelocity) const = 0;
};
