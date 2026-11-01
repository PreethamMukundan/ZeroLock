// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroZiplineMode.h"

#include "Components/SplineComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverDataModelTypes.h"
#include "Movement/Zero_ZiplineActor.h"
#include "Mover/ZeroMovementData.h"


void FZeroZipliningState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("ZiplineActor: %s\n", *GetNameSafe(ZiplineActor));
	Out.Appendf("bMovingToEnd: %d\n", bMovingToEnd);
	Out.Appendf("Distance: %.1f Speed: %.1f\n", Distance, Speed);
	Out.Appendf("LastZipSimTimeMs: %.1f\n", LastZipSimTimeMs);
}

bool FZeroZipliningState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);
	Map->SerializeObject(Ar, AZero_ZiplineActor::StaticClass(), (UObject*&)ZiplineActor);

	uint8 LocalMovingToEnd = bMovingToEnd;
	Ar.SerializeBits(&LocalMovingToEnd, 1);

	if (Ar.IsLoading())
	{
		bMovingToEnd = LocalMovingToEnd;
	}
	Ar << Distance;
	Ar << Speed;
	Ar << LastZipSimTimeMs;
	bOutSuccess = true;
	return true;
}

bool FZeroZipliningState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FZeroZipliningState* AuthState = static_cast<const FZeroZipliningState*>(&AuthorityState);
	return ZiplineActor != AuthState->ZiplineActor
		|| bMovingToEnd != AuthState->bMovingToEnd
		|| !FMath::IsNearlyEqual(Distance, AuthState->Distance, 10.0f)
		|| !FMath::IsNearlyEqual(Speed, AuthState->Speed, 10.0f);
}

void FZeroZipliningState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FZeroZipliningState* FromState = static_cast<const FZeroZipliningState*>(&From);
	const FZeroZipliningState* ToState = static_cast<const FZeroZipliningState*>(&To);
	ZiplineActor = ToState->ZiplineActor;
	bMovingToEnd = ToState->bMovingToEnd;
	LastZipSimTimeMs = ToState->LastZipSimTimeMs;
	const bool bSameRide = FromState->ZiplineActor == ToState->ZiplineActor && FromState->bMovingToEnd == ToState->bMovingToEnd;
	Distance = bSameRide ? FMath::Lerp(FromState->Distance, ToState->Distance, Pct) : ToState->Distance;
	Speed = bSameRide ? FMath::Lerp(FromState->Speed, ToState->Speed, Pct) : ToState->Speed;
}

UScriptStruct* FZeroZipliningState::GetScriptStruct() const
{
	return FZeroZipliningState::StaticStruct();
}

UZeroZiplineMode::UZeroZiplineMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AZero_ZiplineActor* UZeroZiplineMode::FindZiplineInView(const UWorld* World, const FVector& Eye, const FVector& LookDir,
	float MaxDistance, float TraceRadius, const AActor* IgnoreActor, float& OutDistance)
{
	if (!World || LookDir.IsNearlyZero())
	{
		return nullptr;
	}

	TArray<FHitResult> Hits;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZiplineLook), false, IgnoreActor);
	const FVector Dir = LookDir.GetSafeNormal();
	World->SweepMultiByObjectType(Hits, Eye, Eye + Dir * MaxDistance, FQuat::Identity, FCollisionObjectQueryParams(ECC_Vehicle),
		FCollisionShape::MakeSphere(TraceRadius), QueryParams);

	// Hits come sorted by distance; the first zipline wins.
	for (const FHitResult& Hit : Hits)
	{
		AZero_ZiplineActor* Zip = Cast<AZero_ZiplineActor>(Hit.GetActor());
		USplineComponent* Spline = Zip ? Zip->GetZiplineComponent().Get() : nullptr;
		if (Spline)
		{
			const FVector Point = Hit.bStartPenetrating ? Eye : Hit.ImpactPoint;
			// The sweep only sees cables; don't grab one behind a wall or a block (the pull up would hit it).
			FHitResult Blocker;
			if (World->LineTraceSingleByChannel(Blocker, Eye, Point, ECC_Visibility, QueryParams) && Blocker.GetActor() != Zip)
			{
				return nullptr;
			}
			OutDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(Point));
			return Zip;
		}
	}
	return nullptr;
}

void UZeroZiplineMode::SimulationTick_Implementation(const FSimulationTickParams& Params,
	FMoverTickEndData& OutputState)
{
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	FZeroZipliningState& OutZipState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FZeroZipliningState>();
	const FZeroZipliningState* StartZipState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FZeroZipliningState>();
	const FMoverDefaultSyncState* StartSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
	const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	if (!UpdatedComponent)
	{
		return;
	}

	const double NowMs = Params.TimeStep.BaseSimTimeMs;
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector StartLoc = UpdatedComponent->GetComponentLocation();
	const FRotator StartRot = UpdatedComponent->GetComponentRotation();
	const FVector StartVelocity = StartSyncState ? StartSyncState->GetVelocity_WorldSpace() : FVector::ZeroVector;

	if (StartZipState)
	{
		OutZipState = *StartZipState;
	}

	// Leave the cable, keeping the given velocity for the next mode.
	auto Detach = [&](const FVector& Velocity, float RemainingMs)
	{
		OutZipState.LastZipSimTimeMs = NowMs;
		OutputSyncState.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentRotation(), Velocity, FVector::ZeroVector);
		UpdatedComponent->ComponentVelocity = Velocity;
		OutputState.MovementEndState.NextModeName = ExitModeName;
		OutputState.MovementEndState.RemainingMs = RemainingMs;
	};

	// A gap since the last zipline step (or no ride yet) is a fresh grab: find the cable again, never reuse an old one.
	const bool bFreshGrab = !StartZipState || !StartZipState->ZiplineActor
		|| NowMs - StartZipState->LastZipSimTimeMs > Params.TimeStep.StepMs * 1.5;
	if (bFreshGrab)
	{
		float GrabDistance = 0.0f;
		const FVector LookVector = ZeroInputs ? ZeroInputs->LookDir.Vector() : UpdatedComponent->GetForwardVector();
		AZero_ZiplineActor* Zip = FindZiplineInView(GetWorld(), StartLoc + FVector::UpVector * EyeHeight, LookVector,
			LookMaxDistance, LookTraceRadius, UpdatedComponent->GetOwner(), GrabDistance);
		USplineComponent* Spline = Zip ? Zip->GetZiplineComponent().Get() : nullptr;
		if (!Spline)
		{
			OutZipState.ZiplineActor = nullptr;
			Detach(StartVelocity, Params.TimeStep.StepMs);
			return;
		}

		// Ride toward where the camera looks; near an end, ride away from it.
		const float Length = Spline->GetSplineLength();
		const FVector Tangent = Spline->GetDirectionAtDistanceAlongSpline(GrabDistance, ESplineCoordinateSpace::World);
		bool bToEnd = (LookVector | Tangent) >= 0.0f;
		if (GrabDistance < 200.0f)
		{
			bToEnd = true;
		}
		else if (GrabDistance > Length - 200.0f)
		{
			bToEnd = false;
		}

		OutZipState.ZiplineActor = Zip;
		OutZipState.bMovingToEnd = bToEnd;
		OutZipState.Distance = GrabDistance;
		OutZipState.Speed = FMath::Clamp(StartVelocity | (Tangent * (bToEnd ? 1.0f : -1.0f)), MinEntrySpeed, ZiplineSpeed);
	}
	OutZipState.LastZipSimTimeMs = NowMs;

	const AZero_ZiplineActor* Zip = Cast<AZero_ZiplineActor>(OutZipState.ZiplineActor);
	USplineComponent* Spline = Zip ? Zip->GetZiplineComponent().Get() : nullptr;
	if (!Spline)
	{
		Detach(StartVelocity, Params.TimeStep.StepMs);
		return;
	}

	const float Dir = OutZipState.bMovingToEnd ? 1.0f : -1.0f;
	const FVector RideDir = Spline->GetDirectionAtDistanceAlongSpline(OutZipState.Distance, ESplineCoordinateSpace::World) * Dir;

	// Jump is handled by the component's jump impulse (it switches to the air mode keeping this velocity); crouch drops off.
	if (ZeroInputs && (ZeroInputs->bWantsToCrouch || ZeroInputs->bCustomJumpJustPressed))
	{
		Detach(RideDir * OutZipState.Speed, Params.TimeStep.StepMs);
		return;
	}

	OutZipState.Speed = FMath::Min(ZiplineSpeed, OutZipState.Speed + ZiplineAcceleration * DeltaSeconds);
	const float Length = Spline->GetSplineLength();
	const float NextDistance = OutZipState.Distance + Dir * OutZipState.Speed * DeltaSeconds;
	const bool bReachedEnd = NextDistance >= Length || NextDistance <= 0.0f;
	OutZipState.Distance = FMath::Clamp(NextDistance, 0.0f, Length);

	// Move toward the hang point; the part beyond this step's travel is the pull up to the cable, capped at AttachSpeed.
	const FVector HangPoint = Spline->GetLocationAtDistanceAlongSpline(OutZipState.Distance, ESplineCoordinateSpace::World)
		- FVector::UpVector * HangOffset;
	FVector Delta = HangPoint - StartLoc;
	const float MaxStep = (OutZipState.Speed + AttachSpeed) * DeltaSeconds;
	if (Delta.SizeSquared() > FMath::Square(MaxStep))
	{
		Delta = Delta.GetSafeNormal() * MaxStep;
	}

	// Face where the player looks (aim and abilities use the actor rotation), else along the cable.
	FVector Facing = DefaultInputs ? DefaultInputs->GetOrientationIntentDir_WorldSpace() : FVector::ZeroVector;
	Facing.Z = 0.0f;
	if (!Facing.Normalize())
	{
		Facing = FVector(RideDir.X, RideDir.Y, 0.0f).GetSafeNormal();
	}
	const FQuat NewRotation = Facing.IsNearlyZero() ? StartRot.Quaternion() : Facing.ToOrientationQuat();

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);
	FHitResult Hit(1.0f);
	UMovementUtils::TrySafeMoveUpdatedComponent(Params.MovingComps, Delta, NewRotation, true, Hit, ETeleportType::None, MoveRecord);

	const FVector RideVelocity = RideDir * OutZipState.Speed;
	OutputState.SyncState.MovementMode = ZipliningModeName;

	// Something solid in the way (a wall, a platform under the cable): let go instead of sticking.
	if (Hit.IsValidBlockingHit() && Hit.Time < 0.05f)
	{
		Detach(RideVelocity, 0.0f);
		return;
	}
	if (bReachedEnd)
	{
		Detach(RideVelocity, 0.0f);
		return;
	}

	OutputSyncState.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentRotation(), RideVelocity, FVector::ZeroVector);
	UpdatedComponent->ComponentVelocity = RideVelocity;
}
