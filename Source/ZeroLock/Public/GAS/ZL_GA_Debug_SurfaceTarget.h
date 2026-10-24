// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseSurfaceTargetAbility.h"
#include "ZL_GA_Debug_SurfaceTarget.generated.h"

/**
 * Test ability for UZL_BaseSurfaceTargetAbility / AZL_GATA_SurfaceTrace: aim, confirm, then draws the confirmed
 * location and normal and logs it on the client and the server.
 */
UCLASS()
class ZEROLOCK_API UZL_GA_Debug_SurfaceTarget : public UZL_BaseSurfaceTargetAbility
{
	GENERATED_BODY()
public:
	UZL_GA_Debug_SurfaceTarget();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	float DrawDuration = 5.f;

	virtual void AbilityCancelledAction(const FGameplayAbilityTargetDataHandle& Data) override;

protected:
	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data) override;
};
