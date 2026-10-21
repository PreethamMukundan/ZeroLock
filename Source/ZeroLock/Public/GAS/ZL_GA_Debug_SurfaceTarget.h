// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/Base_GA_TargetActors.h"
#include "GAS/TargetActors/ZL_GATA_SurfaceTrace.h"
#include "ZL_GA_Debug_SurfaceTarget.generated.h"

/**
 * Test ability for AZL_GATA_SurfaceTrace: aim, confirm, then draws the confirmed location and normal
 * and logs it on the client and the server.
 */
UCLASS()
class ZEROLOCK_API UZL_GA_Debug_SurfaceTarget : public UBase_GA_TargetActors
{
	GENERATED_BODY()
public:
	UZL_GA_Debug_SurfaceTarget();

	/** Applied to the spawned surface trace actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	EZL_SurfaceTargetMode SurfaceMode = EZL_SurfaceTargetMode::Any;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	float MaxRange = 2000.f;

	/** Draws the live trace (green valid / red invalid) while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	bool bDrawLiveTrace = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	float DrawDuration = 5.f;

	virtual void OnTargetActorSpawned(AGameplayAbilityTargetActor_Trace* TargetActor) override;
	virtual void AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data) override;
	virtual void AbilityCancelledAction(const FGameplayAbilityTargetDataHandle& Data) override;
};
