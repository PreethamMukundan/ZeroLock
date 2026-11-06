// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ZeroZiplineTransition.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroZiplineTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()
	
public:
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	// Grab only the cable the player looks at: a sphere sweep from the eye (pawn centre + EyeHeight) along the look
	// direction. GrabMaxDistance reaches a lane cable (750 cm) from the ground or a nearby roof; you get pulled up to it.
	// Keep these a little below UZeroZiplineMode's LookMaxDistance / LookTraceRadius.
	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	float GrabMaxDistance = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	float GrabTraceRadius = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	float EyeHeight = 60.0f;

	// No new grab this soon after leaving a cable (the zipline key is read every frame while held, so a jump off would regrab).
	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	float RegrabCooldownMs = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	FName ZipliningModeName = TEXT("Ziplining");
	
};


UCLASS(Blueprintable, BlueprintType)
class ZEROLOCK_API UZeroZiplineEndTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()
public:
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	UPROPERTY(EditAnywhere, Category = "Exit Settings")
	FName AutoExitToMode = TEXT("Falling");
};
