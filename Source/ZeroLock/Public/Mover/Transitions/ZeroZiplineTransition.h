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

	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	float ZiplineCheckSphereRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Trace Settings")
	float ZiplineCheckMaxDistance = 500.0f;

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
