// Copyright Preetham Mukundan (C) 2026


#include "Mover/Transitions/ZeroJumpTransition.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"

#include"MoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "ZeroLock/ZeroLock.h"

UZeroJumpTransition::UZeroJumpTransition()
{
}

FTransitionEvalResult UZeroJumpTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FCharacterDefaultInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();

	if (Inputs && Inputs->bIsJumpJustPressed)
	{
		// 5.6 API: Just pass the FName of the mode you want to enter
		return FTransitionEvalResult(FName("Falling"));
	}

	// 5.6 API: Use the built-in static variable to gracefully fail the transition
	return FTransitionEvalResult::NoTransition;
}

void UZeroJumpTransition::Trigger_Implementation(const FSimulationTickParams& Params)
{
	Super::Trigger_Implementation(Params);
	ZLOG("jumped");
}


