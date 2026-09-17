// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroDashMode.h"

#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroMoverPawn.h"

void UZeroDashMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
const FZeroMovementInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
    const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

    const UZeroMovementSettings* Settings = nullptr;
    if (AZeroMoverPawn* Pawn = Cast<AZeroMoverPawn>(GetMoverComponent()->GetOwner())) 
    { 
        //Settings = Pawn->SlideSettings; 
    }

    if (!Settings || !StartingSyncState)
    {
       OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
       return;
    }

    OutputState.SyncState.MovementMode = TEXT("Dashing");
    FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
    
    FVector DashDirection = FVector::ZeroVector;

    if (Inputs && !Inputs->MoveInput.IsNearlyZero())
    {
        FVector ForwardVector = Params.MovingComps.UpdatedComponent->GetForwardVector().GetSafeNormal2D();
        FVector RightVector = Params.MovingComps.UpdatedComponent->GetRightVector().GetSafeNormal2D();

        DashDirection = (ForwardVector * Inputs->MoveInput.X) + (RightVector * Inputs->MoveInput.Y);
        DashDirection = DashDirection.GetSafeNormal2D();
    }
    else
    {
        DashDirection = Params.MovingComps.UpdatedComponent->GetForwardVector().GetSafeNormal2D();
    }
    
    FVector Velocity = DashDirection * Settings->DashSpeed;
    

    Velocity.Z = 0.0f; 

    OutputSyncState.SetTransforms_WorldSpace(
       Params.MovingComps.UpdatedComponent->GetComponentLocation(),
       Params.MovingComps.UpdatedComponent->GetComponentRotation(),
       Velocity, nullptr
    );


    FZeroDashState& OutputDashState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FZeroDashState>();
    float CurrentSimTime = Params.TimeStep.BaseSimTimeMs * 0.001f;
    OutputDashState.NextDashAvailableTime = CurrentSimTime + Settings->DashCooldown;


    OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
}
