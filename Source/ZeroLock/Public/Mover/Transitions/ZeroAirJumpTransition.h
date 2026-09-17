// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ZeroAirJumpTransition.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroAirJumpTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()
	
	protected:
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	
};
