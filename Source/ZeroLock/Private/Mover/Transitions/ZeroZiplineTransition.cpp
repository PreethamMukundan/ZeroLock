// Copyright Preetham Mukundan (C) 2026


#include "Mover/Transitions/ZeroZiplineTransition.h"

#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroZiplineMode.h"

FTransitionEvalResult UZeroZiplineTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
	UMoverComponent* MoverComp = Params.MovingComps.MoverComponent.Get();
	if (!MoverComp || Params.StartState.SyncState.MovementMode == ZipliningModeName)
	{
		return EvalResult;
	}

	const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
	// Component-level transitions run in every mode, including the rooted one; mounting would break the root.
	if (!ZeroInputs || !ZeroInputs->bWantsToZipline || ZeroInputs->bIsRooted || ZeroInputs->bIsCubed || ZeroInputs->bIsGooBall)
	{
		return EvalResult;
	}

	if (const FZeroZipliningState* ZipState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FZeroZipliningState>())
	{
		if (ZipState->LastZipSimTimeMs >= 0.0 && Params.TimeStep.BaseSimTimeMs - ZipState->LastZipSimTimeMs < RegrabCooldownMs)
		{
			return EvalResult;
		}
	}

	AActor* OwnerActor = MoverComp->GetOwner();
	float Distance = 0.0f;
	if (UZeroZiplineMode::FindZiplineInView(GetWorld(), OwnerActor->GetActorLocation() + FVector::UpVector * EyeHeight,
		ZeroInputs->LookDir.Vector(), GrabMaxDistance, GrabTraceRadius, OwnerActor, Distance))
	{
		EvalResult.NextMode = ZipliningModeName;
	}
	return EvalResult;
}


FTransitionEvalResult UZeroZiplineEndTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
	
	if (const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>())
	{

		if (ZeroInputs->bCustomJumpJustPressed || ZeroInputs->bWantsToCrouch) 
		{
			EvalResult.NextMode = AutoExitToMode;
		}
	}

	return EvalResult;
}
