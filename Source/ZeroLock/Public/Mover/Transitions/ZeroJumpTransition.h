// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ZeroJumpTransition.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, EditInlineNew)
class ZEROLOCK_API UZeroJumpTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()
public:
	UZeroJumpTransition();

	UPROPERTY(EditAnywhere, Category = "Zero Jump")
	float JumpZVelocity = 600.0f;

	// Use _Implementation for BlueprintNativeEvents
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	
};
