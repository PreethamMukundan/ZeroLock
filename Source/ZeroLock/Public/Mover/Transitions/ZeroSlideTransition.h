// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ZeroSlideTransition.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroSlideTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()

public:
	UZeroSlideTransition(const FObjectInitializer& ObjectInitializer);

	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

public:
	/** Name of the sliding mode to transition into */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	FName SlidingModeName = TEXT("Sliding");

	/** Minimum horizontal speed required to successfully initiate a slide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions", meta = (ForceUnits = "cm/s"))
	float MinSpeedToSlide = 300.0f;
};
