// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroGooBallMode.h"

#include "MoverComponent.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ZeroGooBallMode)

void UZeroGooBallMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// Momentum only; rolling, gravity and bounces are applied in SimulationTick, where the floor is known.
	// Layered moves (pulls, stops) override this before SimulationTick.
	if (const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		OutProposedMove.LinearVelocity = SyncState->GetVelocity_WorldSpace();
	}
}

void UZeroGooBallMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
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

	// World-space move intent from the input (already scaled by slows), flattened.
	FVector Intent = FVector::ZeroVector;
	if (const FCharacterDefaultInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		Intent = Inputs->GetMoveInput_WorldSpace();
		Intent.Z = 0.0f;
		Intent = Intent.GetClampedToMaxSize(1.0f);
	}

	FFloorCheckResult Floor;
	UFloorQueryUtils::FindFloor(Params.MovingComps, GroundSnapDistance + 2.0f, GroundMinNormalZ, false, UpdatedComponent->GetComponentLocation(), Floor);
	const FVector FloorNormal = Floor.HitResult.ImpactNormal;
	const bool bOnGround = Floor.bBlockingHit && Floor.FloorDist <= GroundSnapDistance && FloorNormal.Z >= GroundMinNormalZ
		&& FVector::DotProduct(Velocity, FloorNormal) <= MinBounceSpeed;

	if (bOnGround)
	{
		if (FVector::DotProduct(Velocity, FloorNormal) < 0.0f)
		{
			Velocity = FVector::VectorPlaneProject(Velocity, FloorNormal);
		}
		// Slopes: gravity along the surface.
		Velocity += FVector::VectorPlaneProject(Gravity, FloorNormal) * DeltaSeconds;

		const FVector Desired = FVector::VectorPlaneProject(Intent, FloorNormal).GetSafeNormal();
		const float InputStrength = Intent.Size();
		const float Speed = Velocity.Size();
		if (InputStrength > KINDA_SMALL_NUMBER && !Desired.IsNearlyZero())
		{
			if (Speed > 1.0f)
			{
				// Heavy ball: the rolling direction turns toward the input at a limited rate.
				const FVector Dir = Velocity / Speed;
				const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir, Desired), -1.0f, 1.0f));
				const float MaxTurn = FMath::DegreesToRadians(TurnRate) * DeltaSeconds;
				if (Angle > KINDA_SMALL_NUMBER)
				{
					const FVector Axis = FVector::CrossProduct(Dir, Desired).GetSafeNormal();
					const FVector TurnedDir = Axis.IsNearlyZero() ? Dir : Dir.RotateAngleAxisRad(FMath::Min(Angle, MaxTurn), Axis);
					Velocity = TurnedDir * Speed;
				}
				// Steering against the roll brakes it.
				if (FVector::DotProduct(Dir, Desired) < 0.0f)
				{
					Velocity *= FMath::Max(Speed - BrakeDeceleration * InputStrength * DeltaSeconds, 0.0f) / Speed;
				}
			}
			// Push along the input, but input alone can't exceed MaxRollSpeed.
			if (FVector::DotProduct(Velocity, Desired) < MaxRollSpeed)
			{
				Velocity += Desired * (RollAcceleration * InputStrength * DeltaSeconds);
			}
		}
		else if (Speed > 0.0f)
		{
			Velocity *= FMath::Max(Speed - RollFriction * DeltaSeconds, 0.0f) / Speed;
		}
	}
	else
	{
		Velocity += Gravity * DeltaSeconds;
		Velocity += Intent * (AirControl * DeltaSeconds);
	}

	const FQuat Rotation = StartSync->GetOrientation_WorldSpace().Quaternion();
	const FVector IncomingVelocity = Velocity;
	FHitResult Hit(1.0f);
	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);
	UMovementUtils::TrySafeMoveAndSlideUpdatedComponent(Params.MovingComps, Velocity * DeltaSeconds, Rotation, true, Hit, ETeleportType::None, MoveRecord, true);

	if (Hit.bBlockingHit)
	{
		const float IntoSurface = FVector::DotProduct(IncomingVelocity, Hit.Normal);
		if (IntoSurface < 0.0f)
		{
			// Walls and ceilings bounce hard; landings bounce only when they're fast.
			const bool bWall = Hit.Normal.Z < GroundMinNormalZ;
			const float Bounce = bWall ? WallRestitution : (-IntoSurface > MinBounceSpeed ? FloorRestitution : 0.0f);
			Velocity = IncomingVelocity - (1.0f + Bounce) * IntoSurface * Hit.Normal;
		}
	}

	// Face the way it rolls (the hero mesh is hidden; this keeps the camera-independent facing sensible on exit).
	FRotator Facing = Rotation.Rotator();
	if (Velocity.SizeSquared2D() > FMath::Square(50.0f))
	{
		Facing = FRotator(0.0f, Velocity.Rotation().Yaw, 0.0f);
	}

	OutSync.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), Facing, Velocity, FVector::ZeroVector, nullptr);
	UpdatedComponent->ComponentVelocity = Velocity;
}
