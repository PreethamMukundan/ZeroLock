// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroDashMode.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroDashMode : public UBaseMovementMode
{
	GENERATED_BODY()
	
public:
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	
};
