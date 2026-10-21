// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ZL_ProjectileMovementComponent.generated.h"

/**
 * Projectile movement used by AProjectile. A blocking hit on an IZL_ProjectilePortal moves the projectile to the
 * portal's exit (velocity redirected) and the rest of the sub-step continues from there.
 */
UCLASS()
class ZEROLOCK_API UZL_ProjectileMovementComponent : public UProjectileMovementComponent
{
	GENERATED_BODY()

public:
	/** Portals passed so far. The fake and authoritative copies only compare positions while these match. */
	int32 GetPortalTraversals() const { return PortalTraversals; }

	/** For projectiles that move themselves (Luggage Cart) and pass a portal outside this component. */
	void AddPortalTraversal() { PortalTraversals++; }

protected:
	int32 PortalTraversals = 0;

	virtual EHandleBlockingHitResult HandleBlockingHit(const FHitResult& Hit, float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining) override;
};
