// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ZeroWallJumpTransition.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroWallJumpTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()
	
	protected:
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	
};
