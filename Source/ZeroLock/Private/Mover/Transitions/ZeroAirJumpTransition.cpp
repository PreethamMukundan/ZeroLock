// Copyright Preetham Mukundan (C) 2026


#include "Mover/Transitions/ZeroAirJumpTransition.h"

#include "MoverSimulationTypes.h"
#include "Mover/ZeroMovementData.h"

FTransitionEvalResult UZeroAirJumpTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
	const FZeroMovementInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();

	if (!Inputs) return EvalResult;
	
	else if (Params.StartState.SyncState.MovementMode == DefaultModeNames::Walking && Inputs->bCustomJumpJustPressed)
	{
		EvalResult.NextMode = TEXT("AirJumping");
	}
	return EvalResult;
}
