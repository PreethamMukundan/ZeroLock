// Copyright Preetham Mukundan (C) 2026


#include "Weapon/ZL_ProjectileMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Weapon/ZL_ProjectilePortal.h"

UProjectileMovementComponent::EHandleBlockingHitResult UZL_ProjectileMovementComponent::HandleBlockingHit(const FHitResult& Hit, float TimeTick,
	const FVector& MoveDelta, float& SubTickTimeRemaining)
{
	if (UpdatedComponent && !Hit.bStartPenetrating)
	{
		if (const IZL_ProjectilePortal* Portal = Cast<IZL_ProjectilePortal>(Hit.GetActor()))
		{
			FVector ExitLocation;
			FVector ExitVelocity;
			const float Radius = UpdatedPrimitive ? UpdatedPrimitive->GetCollisionShape().GetExtent().GetMax() : 0.f;
			if (Portal->GetPortalExit(Hit, Velocity, Radius, ExitLocation, ExitVelocity))
			{
				PortalTraversals++;
				UpdatedComponent->SetWorldLocationAndRotation(ExitLocation, bRotationFollowsVelocity ? ExitVelocity.Rotation() : UpdatedComponent->GetComponentRotation(),
					false, nullptr, ETeleportType::TeleportPhysics);
				Velocity = ExitVelocity;
				UpdateComponentVelocity();

				// Spend the rest of this sub-step on the other side.
				SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
				return EHandleBlockingHitResult::AdvanceNextSubstep;
			}
		}
	}

	return Super::HandleBlockingHit(Hit, TimeTick, MoveDelta, SubTickTimeRemaining);
}
