// Copyright Preetham Mukundan (C) 2026


#include "Mover/Transitions/ZeroWallJumpTransition.h"

#include "MoverSimulationTypes.h"
#include "Components/CapsuleComponent.h"
#include "Mover/ZeroMovementData.h"

FTransitionEvalResult UZeroWallJumpTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
	const FZeroMovementInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();

	if (!Inputs || !Inputs->bCustomJumpJustPressed || Params.StartState.SyncState.MovementMode != DefaultModeNames::Falling) 
	{
		return EvalResult;
	}

	// We pressed jump in the air. Are we near a wall?
	if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Params.MovingComps.UpdatedComponent.Get()))
	{
		FVector Start = Capsule->GetComponentLocation();
		// Sweep slightly larger than our capsule to detect walls brushing against us
		float CheckRadius = Capsule->GetScaledCapsuleRadius() + 30.0f; 
		
		FCollisionShape Sphere = FCollisionShape::MakeSphere(CheckRadius);
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(Capsule->GetOwner());

		FHitResult Hit;
		bool bHitWall = Capsule->GetWorld()->SweepSingleByProfile(
			Hit, Start, Start, FQuat::Identity, TEXT("BlockAllDynamic"), Sphere, TraceParams
		);

		// If we hit something, and it's mostly vertical (a wall, not a floor)
		if (bHitWall && FMath::Abs(Hit.Normal.Z) < 0.3f)
		{
			EvalResult.NextMode = TEXT("WallJumping");
		}
	}
	return EvalResult;
}
