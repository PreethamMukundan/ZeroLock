// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroNoClipMode.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "Mover/ZeroMovementData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ZeroNoClipMode)

void UZeroNoClipMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const FCharacterDefaultInputs* DefaultInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FZeroMovementInputs* ZeroInputs = StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
	if (!DefaultInputs || !ZeroInputs)
	{
		OutProposedMove.LinearVelocity = FVector::ZeroVector;
		return;
	}

	// The move input is a flat world direction built from the control yaw (AZeroMoverPawn::ProduceInput); take its
	// forward/right parts back out and point forward along the full look direction instead.
	const FRotator Look = ZeroInputs->LookDir;
	const FRotator YawOnly(0.0f, Look.Yaw, 0.0f);
	const FVector FlatForward = YawOnly.Vector();
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	const FVector MoveInput = DefaultInputs->GetMoveInput();

	FVector Direction = Look.Vector() * FVector::DotProduct(MoveInput, FlatForward) + Right * FVector::DotProduct(MoveInput, Right);
	Direction.Z += (ZeroInputs->bJumpHold ? 1.0f : 0.0f) - (ZeroInputs->bWantsToCrouch ? 1.0f : 0.0f);

	OutProposedMove.LinearVelocity = Direction.GetSafeNormal() * Speed;
}

void UZeroNoClipMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	if (!UpdatedComponent || DeltaSeconds <= 0.0f)
	{
		return;
	}

	OutputState.SyncState.MovementMode = Params.StartState.SyncState.MovementMode;
	FMoverDefaultSyncState& OutSync = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	// Face the look yaw (from the input, so client and server agree).
	FRotator Facing = UpdatedComponent->GetComponentRotation();
	if (const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>())
	{
		Facing = FRotator(0.0f, ZeroInputs->LookDir.Yaw, 0.0f);
	}

	// No sweep: walls, floors and other pawns don't stop it.
	const FVector Velocity = Params.ProposedMove.LinearVelocity;
	const FVector NewLocation = UpdatedComponent->GetComponentLocation() + Velocity * DeltaSeconds;
	UpdatedComponent->SetWorldLocationAndRotation(NewLocation, Facing.Quaternion(), false, nullptr, ETeleportType::None);

	OutSync.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), Facing, Velocity, FVector::ZeroVector, nullptr);
	UpdatedComponent->ComponentVelocity = Velocity;
}
