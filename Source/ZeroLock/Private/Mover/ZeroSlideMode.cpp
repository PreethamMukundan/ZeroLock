// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroSlideMode.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Mover/ZeroMovementData.h"
#include "ZeroLock/ZeroLock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ZeroSlideMode)

UZeroSlideMode::UZeroSlideMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SlideInitialSpeedBoost = 500.0f;
	SlideGravityForce = 1200.0f;
	SlideSteeringAuthority = 2500.0f; 
	SlideFriction = 0.5f; 
	SlideMinSpeed = 150.0f;
	SlideJumpImpulse = 650.0f;
}


void UZeroSlideMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

    const FZeroMovementInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
    const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

    if (!StartingSyncState || !Params.MovingComps.UpdatedComponent.IsValid())
    {
       OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
       return;
    }

    USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
    OutputState.SyncState.MovementMode = TEXT("Sliding");
    FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
    
    FVector CurrentVelocity = StartingSyncState->GetVelocity_WorldSpace();


    if (Params.StartState.SyncState.MovementMode != TEXT("Sliding"))
    {
       FVector BoostDir = FVector::ZeroVector;
       
       if (DefaultInputs && !DefaultInputs->GetMoveInput().IsNearlyZero())
       {
           BoostDir = DefaultInputs->GetMoveInput().GetSafeNormal2D();
       }
       else if (CurrentVelocity.SizeSquared2D() > 100.0f)
       {
           BoostDir = CurrentVelocity.GetSafeNormal2D();
       }
       else if (Inputs)
       {
           BoostDir = Inputs->LookDir.Vector().GetSafeNormal2D();
       }

       CurrentVelocity += BoostDir * SlideInitialSpeedBoost;
    }

    FRotator TargetOrient = UpdatedComponent->GetComponentRotation();
    if (Inputs)
    {
       TargetOrient = Inputs->LookDir.Vector().ToOrientationRotator();
    }
    TargetOrient.Pitch = 0.0f;
    TargetOrient.Roll = 0.0f;
    FQuat NewRot = TargetOrient.Quaternion();
   

    FVector CumulativeForces = FVector::DownVector * SlideGravityForce;

    FVector LookDir2D = Inputs ? Inputs->LookDir.Vector().GetSafeNormal2D() : FVector::ForwardVector;
    FVector VelDir2D = CurrentVelocity.GetSafeNormal2D();

    bool bIsMovingBackward = FVector::DotProduct(VelDir2D, LookDir2D) < -0.2f;


    if (DefaultInputs && !DefaultInputs->GetMoveInput().IsNearlyZero() && Inputs)
    {

       FVector RightVec = FRotationMatrix(TargetOrient).GetUnitAxis(EAxis::Y);

       float SteeringDot = FVector::DotProduct(DefaultInputs->GetMoveInput().GetSafeNormal2D(), RightVec);

       float FinalSteeringAuthority = bIsMovingBackward ? -SlideSteeringAuthority : SlideSteeringAuthority;

       CumulativeForces += RightVec * (SteeringDot * FinalSteeringAuthority);
    }

    if (Inputs && CurrentVelocity.Size2D() > 10.0f)
    {
       float Speed2D = CurrentVelocity.Size2D();
       
       FVector TargetDir2D = bIsMovingBackward ? -LookDir2D : LookDir2D;

       FVector BlendedDir = FMath::VInterpConstantTo(VelDir2D, TargetDir2D, DeltaSeconds, 5.0f);
       
       CurrentVelocity = BlendedDir.GetSafeNormal2D() * Speed2D + FVector(0, 0, CurrentVelocity.Z);
    }

    CurrentVelocity += CumulativeForces * DeltaSeconds;

    float Speed = CurrentVelocity.Size2D();
    if (Speed > 0.0f)
    {
       float FrictionDrop = SlideFriction * 500.0f * DeltaSeconds; 
       float NewSpeed = FMath::Max(Speed - FrictionDrop, 0.0f);
       CurrentVelocity = CurrentVelocity.GetSafeNormal2D() * NewSpeed + FVector(0, 0, CurrentVelocity.Z);
    }

    FVector OldLocation = UpdatedComponent->GetComponentLocation();
    FVector MoveDelta = CurrentVelocity * DeltaSeconds;
    
    FHitResult Hit(1.f);
    FMovementRecord MoveRecord;
    MoveRecord.SetDeltaSeconds(DeltaSeconds);
   
    UMovementUtils::TrySafeMoveAndSlideUpdatedComponent(Params.MovingComps, MoveDelta, NewRot, true, Hit, ETeleportType::None, MoveRecord, true);


    if (DeltaSeconds > 0.0f)
    {
       CurrentVelocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaSeconds;
    }


    if (Hit.bBlockingHit && Hit.Normal.Z > 0.01f && Hit.Normal.Z < 0.99f)
    {
       FVector SlopeDownwardDir = FVector::VectorPlaneProject(FVector::DownVector, Hit.Normal).GetSafeNormal();

       if (FVector::DotProduct(CurrentVelocity, SlopeDownwardDir) > 0.0f)
       {
          CurrentVelocity += SlopeDownwardDir * (SlideGravityForce * 1.5f) * DeltaSeconds;
       }
    }


    const bool bWantsToCrouch = Inputs && Inputs->bWantsToCrouch;

    if (!bWantsToCrouch || CurrentVelocity.SizeSquared2D() < FMath::Square(SlideMinSpeed))
    {
       OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
    }

    OutputSyncState.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentRotation(), CurrentVelocity, FVector::ZeroVector, nullptr);
    UpdatedComponent->ComponentVelocity = CurrentVelocity;
}