// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroMeleeMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "MoveLibrary/MovementUtils.h"
#include "Mover/ZeroMovementData.h"

void FZeroMeleeSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
    const FZeroMeleeSyncState& FromState = static_cast<const FZeroMeleeSyncState&>(From);
    const FZeroMeleeSyncState& ToState = static_cast<const FZeroMeleeSyncState&>(To);
    
    ElapsedTimeMs = FMath::Lerp(FromState.ElapsedTimeMs, ToState.ElapsedTimeMs, Pct);

}

bool FZeroMeleeSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{

    const FZeroMeleeSyncState& AuthMeleeState = static_cast<const FZeroMeleeSyncState&>(AuthorityState);


    const float ErrorTolerance = 1.0f;


    if (FMath::Abs(ElapsedTimeMs - AuthMeleeState.ElapsedTimeMs) > ErrorTolerance)
    {
        return true;
    }


    return false;
}

UZeroMeleeMode::UZeroMeleeMode()
{
}

void UZeroMeleeMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
    const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
    const FZeroMovementInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
    const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

    if (!StartingSyncState || !Params.MovingComps.UpdatedComponent.IsValid() || DeltaSeconds <= 0.0f)
    {
       OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
       return;
    }

    USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
    UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(UpdatedComponent);
    if (!CapsuleComp) return;

    OutputState.SyncState.MovementMode = Params.StartState.SyncState.MovementMode;
    FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

 
    const FZeroMeleeSyncState* StartingMeleeState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FZeroMeleeSyncState>();
    FZeroMeleeSyncState& OutputMeleeState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FZeroMeleeSyncState>();

    if (!StartingMeleeState)
    {
        OutputMeleeState.ElapsedTimeMs = 0.0f;
    }
    else
    {
        OutputMeleeState.ElapsedTimeMs = StartingMeleeState->ElapsedTimeMs + Params.TimeStep.StepMs;
    }

    FVector MeleeDir = FVector::ForwardVector;
    if (Inputs)
    {
        MeleeDir = Inputs->LookDir.Vector();
    }
    MeleeDir.Z = 0.0f;
    MeleeDir.Normalize();

    FVector MoveDelta = MeleeDir * (LungeSpeed * DeltaSeconds);
    FQuat NewRot = FRotationMatrix::MakeFromXZ(MeleeDir, FVector::UpVector).ToQuat();

    FVector OldLocation = UpdatedComponent->GetComponentLocation();
    
    FHitResult EnemyHit;
    FCollisionShape CapShape = CapsuleComp->GetCollisionShape();
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(UpdatedComponent->GetOwner());

    bool bHitEnemy = GetWorld()->SweepSingleByObjectType(EnemyHit,OldLocation,OldLocation + MoveDelta,UpdatedComponent->GetComponentQuat(),FCollisionObjectQueryParams(EnemyTraceChannel),CapShape,TraceParams);

    if (bHitEnemy)
    {
        MoveDelta *= EnemyHit.Time;
    }


    FHitResult WallHit(1.f);
    FMovementRecord MoveRecord;
    MoveRecord.SetDeltaSeconds(DeltaSeconds);

    UMovementUtils::TrySafeMoveAndSlideUpdatedComponent(Params.MovingComps,MoveDelta,NewRot,true,WallHit,ETeleportType::None,MoveRecord,true);


    FVector FinalVelocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaSeconds;

    if (bHitEnemy)
    {
       
        if (!Params.TimeStep.bIsResimulating)
        {
            if (MeleeHitDelegate.IsBound())
            {
                MeleeHitDelegate.Broadcast();
            }
        }
        
        FinalVelocity = FVector::ZeroVector;
        OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
    }
    else if (OutputMeleeState.ElapsedTimeMs >= (MaxDuration * 1000.0f))
    {
        FinalVelocity =FVector::ZeroVector;
        OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
    }
    

    OutputSyncState.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(),NewRot.Rotator(),FinalVelocity,FVector::ZeroVector,nullptr);

    UpdatedComponent->ComponentVelocity = FinalVelocity;
}