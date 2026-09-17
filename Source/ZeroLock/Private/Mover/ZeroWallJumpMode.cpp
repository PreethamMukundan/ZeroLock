// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroWallJumpMode.h"

#include "Components/CapsuleComponent.h"
#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroMoverPawn.h"

void UZeroWallJumpMode::SimulationTick_Implementation(const FSimulationTickParams& Params,
                                                      FMoverTickEndData& OutputState)
{
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

	OutputState.SyncState.MovementMode = TEXT("WallJumping");
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	
	FVector Velocity = StartingSyncState->GetVelocity_WorldSpace();
	FVector WallNormal = FVector::ZeroVector;

	// Redo the sweep to grab the exact wall normal for our physics math
	if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Params.MovingComps.UpdatedComponent.Get()))
	{
		FCollisionShape Sphere = FCollisionShape::MakeSphere(Capsule->GetScaledCapsuleRadius() + 35.0f);
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(Capsule->GetOwner());
		FHitResult Hit;
		if (Capsule->GetWorld()->SweepSingleByProfile(Hit, Capsule->GetComponentLocation(), Capsule->GetComponentLocation(), FQuat::Identity, TEXT("BlockAllDynamic"), Sphere, TraceParams))
		{
			WallNormal = Hit.Normal;
		}
	}

	// 1. Kill any velocity pushing INTO the wall
	if (FVector::DotProduct(Velocity, WallNormal) < 0.0f)
	{
		Velocity = FVector::VectorPlaneProject(Velocity, WallNormal);
	}

	// 2. Apply Wall Jump Forces
	Velocity += WallNormal * Settings->WallJumpOffForce; // Kick away from wall
	Velocity.Z = Settings->WallJumpVerticalForce;        // Kick upward

	OutputSyncState.SetTransforms_WorldSpace(
		Params.MovingComps.UpdatedComponent->GetComponentLocation(),
		Params.MovingComps.UpdatedComponent->GetComponentRotation(),
		Velocity, nullptr
	);

	// Instant exit
	OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
}
