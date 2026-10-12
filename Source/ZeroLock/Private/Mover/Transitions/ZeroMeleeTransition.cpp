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
		// Component-level transitions run in every mode, including the rooted one; a lunge would break the root.
		if (ZeroInputs->bWantsToMelee && !ZeroInputs->bIsRooted)
		{
			EvalResult.NextMode = MeleeModeName;
		}
	}

	return EvalResult;
}
