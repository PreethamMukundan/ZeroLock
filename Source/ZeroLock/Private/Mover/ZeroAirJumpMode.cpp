// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroAirJumpMode.h"

#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroMoverPawn.h"

void UZeroAirJumpMode::SimulationTick_Implementation(const FSimulationTickParams& Params,
                                                     FMoverTickEndData& OutputState)
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
		OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
		return;
	}

	OutputState.SyncState.MovementMode = TEXT("AirJumping");
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	FVector Velocity = StartingSyncState->GetVelocity_WorldSpace();

	// Overwrite Z velocity for crisp double jump
	Velocity.Z = Settings->AirJumpForce;

	// Apply lateral steering boost
	if (Inputs && !Inputs->MoveInput.IsNearlyZero())
	{
		FVector LateralBoost = Inputs->MoveInput.GetSafeNormal2D() * Settings->AirJumpLateralBoost;
		Velocity.X += LateralBoost.X;
		Velocity.Y += LateralBoost.Y;
	}

	OutputSyncState.SetTransforms_WorldSpace(
		Params.MovingComps.UpdatedComponent->GetComponentLocation(),
		Params.MovingComps.UpdatedComponent->GetComponentRotation(),
		Velocity, nullptr
	);

	// Instant exit
	OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
}
