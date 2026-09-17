// Copyright Preetham Mukundan (C) 2026


#include "Mover/Transitions/ZeroDashTransition.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "MoverSimulationTypes.h"
#include "Mover/ZeroMovementData.h"

FTransitionEvalResult UZeroDashTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
    
	const FZeroMovementInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
	if (!Inputs || !Inputs->bWantsToDash) return EvalResult;
	
	const FZeroDashState* DashState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FZeroDashState>();

	float CurrentSimTime = Params.TimeStep.BaseSimTimeMs * 0.001f;

	if (DashState && CurrentSimTime < DashState->NextDashAvailableTime)
	{
		return EvalResult; 
	}
	
	EvalResult.NextMode = TEXT("Dashing");
	return EvalResult;
}
