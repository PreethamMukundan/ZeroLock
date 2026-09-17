// Copyright Preetham Mukundan (C) 2026


#include "Mover/Transitions/ZeroMeleeTransition.h"

#include "MoverSimulationTypes.h"
#include "Mover/ZeroMovementData.h"

UZeroMeleeTransition::UZeroMeleeTransition()
{
}

FTransitionEvalResult UZeroMeleeTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
	
	if (const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>())
	{
		if (ZeroInputs->bWantsToMelee)
		{
			EvalResult.NextMode = MeleeModeName;
		}
	}

	return EvalResult;
}
