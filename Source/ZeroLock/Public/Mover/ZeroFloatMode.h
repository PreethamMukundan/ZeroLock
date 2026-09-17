// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "DefaultMovementSet/Modes/FlyingMode.h"
#include "ZeroFloatMode.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroFloatMode : public UFlyingMode
{
	GENERATED_BODY()
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	

};
