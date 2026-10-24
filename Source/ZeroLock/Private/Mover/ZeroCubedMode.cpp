// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroCubedMode.h"

#include "MoverComponent.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ZeroCubedMode)

// Debug: logs every Nth cubed sim step per machine (role, sim time, location, velocity, floor) to compare client and server.
static TAutoConsoleVariable<int32> CVarCubeLog(TEXT("ZL.Debug.CubeLog"), 0, TEXT("Log the Cubed mover mode every N sim steps (0 = off)."));

void UZeroCubedMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// Momentum only; gravity, slope and friction are applied in SimulationTick, where the floor is known.
	// Layered moves (pulls, stops) override this before SimulationTick.
	if (const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		OutProposedMove.LinearVelocity = SyncState->GetVelocity_WorldSpace();
	}
}

void UZeroCubedMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	const FMoverDefaultSyncState* StartSync = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!UpdatedComponent || !StartSync || DeltaSeconds <= 0.0f)
	{
		return;
	}

	OutputState.SyncState.MovementMode = Params.StartState.SyncState.MovementMode;
	FMoverDefaultSyncState& OutSync = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const UMoverComponent* MoverComp = K2_GetMoverComponent();
	const FVector Gravity = MoverComp ? MoverComp->GetGravityAcceleration() : FVector(0.0f, 0.0f, -980.0f);
	FVector Velocity = Params.ProposedMove.LinearVelocity;

	// Ground: slide down the slope with a little friction. Air: fall.
	FFloorCheckResult Floor;
	UFloorQueryUtils::FindFloor(Params.MovingComps, GroundSnapDistance + 2.0f, GroundMinNormalZ, false, UpdatedComponent->GetComponentLocation(), Floor);
	const FVector FloorNormal = Floor.HitResult.ImpactNormal;
	const bool bOnGround = Floor.bBlockingHit && Floor.FloorDist <= GroundSnapDistance && FloorNormal.Z >= GroundMinNormalZ;
	if (bOnGround)
	{
		if (FVector::DotProduct(Velocity, FloorNormal) < 0.0f)
		{
			Velocity = FVector::VectorPlaneProject(Velocity, FloorNormal);
		}
		Velocity += FVector::VectorPlaneProject(Gravity, FloorNormal) * DeltaSeconds;

		const float Speed = Velocity.Size();
		if (Speed > 0.0f)
		{
			Velocity *= FMath::Max(Speed - Friction * DeltaSeconds, 0.0f) / Speed;
		}
	}
	else
	{
		Velocity += Gravity * DeltaSeconds;
	}

	// Facing from the sync state, not the component: the controller turns the actor outside the simulation, so the
	// component's rotation differs between client and server and every frame would mispredict.
	const FQuat Rotation = StartSync->GetOrientation_WorldSpace().Quaternion();
	FHitResult Hit(1.0f);
	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);
	UMovementUtils::TrySafeMoveAndSlideUpdatedComponent(Params.MovingComps, Velocity * DeltaSeconds, Rotation, true, Hit, ETeleportType::None, MoveRecord, true);

	if (Hit.bBlockingHit)
	{
		const float IntoSurface = FVector::DotProduct(Velocity, Hit.Normal);
		if (IntoSurface < 0.0f)
		{
			// Walls (and ceilings) bounce it back a bit; landing on ground just stops the fall.
			const float Bounce = Hit.Normal.Z < GroundMinNormalZ ? Restitution : 0.0f;
			Velocity -= (1.0f + Bounce) * IntoSurface * Hit.Normal;
		}
	}

	OutSync.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), Rotation.Rotator(), Velocity, FVector::ZeroVector, nullptr);
	UpdatedComponent->ComponentVelocity = Velocity;

	const int32 LogEvery = CVarCubeLog.GetValueOnGameThread();
	if (LogEvery > 0 && (Params.TimeStep.ServerFrame % LogEvery) == 0)
	{
		const AActor* Owner = UpdatedComponent->GetOwner();
		UE_LOG(LogTemp, Warning, TEXT("[CubeLog] %s %s frame %d t %.0f%s loc %s vel %s ground %d floorDist %.1f hit %s"),
			*GetNameSafe(Owner), Owner && Owner->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), Params.TimeStep.ServerFrame,
			Params.TimeStep.BaseSimTimeMs, Params.TimeStep.bIsResimulating ? TEXT(" RESIM") : TEXT(""),
			*UpdatedComponent->GetComponentLocation().ToCompactString(), *Velocity.ToCompactString(), bOnGround ? 1 : 0,
			Floor.FloorDist, Hit.bBlockingHit ? *GetNameSafe(Hit.GetActor()) : TEXT("-"));
	}
}
