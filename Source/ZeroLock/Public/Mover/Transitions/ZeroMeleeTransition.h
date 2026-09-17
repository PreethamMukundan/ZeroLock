// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "ZeroMeleeTransition.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroMeleeTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()
	
public:
	UZeroMeleeTransition();

	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	FName MeleeModeName = TEXT("Melee");
	
};
