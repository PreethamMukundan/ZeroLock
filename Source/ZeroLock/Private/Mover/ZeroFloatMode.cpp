// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroFloatMode.h"

#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/AirMovementUtils.h"
#include "Mover/ZeroMovementData.h"

void UZeroFloatMode::GenerateMove_Implementation(const FMoverSimContext& SimContext,
    const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
    const UMoverComponent* MoverComp = GetMoverComponent();
const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
check(StartingSyncState);

const float DeltaSeconds = TimeStep.StepMs * 0.001f;

FFreeMoveParams Params;

	Params.MoveInputType = EMoveInputType::None;
	Params.MoveInput = FVector::ZeroVector;


FRotator IntendedOrientation_WorldSpace;
// If there's no intent from input to change orientation, use the current orientation
if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
{
	IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();
}
else
{
	IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
}

IntendedOrientation_WorldSpace = UMovementUtils::ApplyGravityToOrientationIntent(IntendedOrientation_WorldSpace, MoverComp->GetWorldToGravityTransform(), CommonLegacySettings->bShouldRemainVertical);

Params.OrientationIntent = IntendedOrientation_WorldSpace;
Params.PriorVelocity = StartingSyncState->GetVelocity_WorldSpace();
Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
Params.TurningRate = CommonLegacySettings->TurningRate;
Params.TurningBoost = CommonLegacySettings->TurningBoost;
Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
Params.Acceleration = CommonLegacySettings->Acceleration;
Params.Deceleration = CommonLegacySettings->Deceleration;
Params.DeltaSeconds = DeltaSeconds;
Params.WorldToGravityQuat = MoverComp->GetWorldToGravityTransform();
Params.bUseAccelerationForVelocityMove = CommonLegacySettings->bUseAccelerationForVelocityMove;

OutProposedMove = UAirMovementUtils::ComputeControlledFreeMove(Params);
}
