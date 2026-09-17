// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ZeroDashTransition.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroDashTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()
	
public:
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
};
