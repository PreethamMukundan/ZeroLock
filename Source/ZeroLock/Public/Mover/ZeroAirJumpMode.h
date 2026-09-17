// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroAirJumpMode.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroAirJumpMode : public UBaseMovementMode
{
	GENERATED_BODY()
	
	protected:
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	
};
