// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroMeleeMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MoveLibrary/MovementUtils.h"
#include "Mover/ZeroMovementData.h"

void FZeroMeleeSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
    const FZeroMeleeSyncState& FromState = static_cast<const FZeroMeleeSyncState&>(From);
    const FZeroMeleeSyncState& ToState = static_cast<const FZeroMeleeSyncState&>(To);
    
    ElapsedTimeMs = FMath::Lerp(FromState.ElapsedTimeMs, ToState.ElapsedTimeMs, Pct);
    StartLocation = ToState.StartLocation;

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

void UZeroMeleeMode::Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext,
    const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
    Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

    if (OutSyncState)
    {
        FZeroMeleeSyncState& MeleeState = OutSyncState->SyncStateCollection.FindOrAddMutableDataByType<FZeroMeleeSyncState>();
        MeleeState.ElapsedTimeMs = 0.0f;
        if (const FMoverDefaultSyncState* DefaultState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
        {
            MeleeState.StartLocation = DefaultState->GetLocation_WorldSpace();
        }
    }
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

    // Look HitReach past this step, and stop HitReach short of the enemy (an enemy we already overlap stops us dead).
    // Hero capsules are object type Hero and ignore each other (we pass through), while BP_Mover_Monkey sets
    // EnemyTraceChannel to Pawn, so query both and only accept another pawn.
    const FVector SweepDelta = MoveDelta + MeleeDir * HitReach;
    FCollisionObjectQueryParams EnemyObjects(EnemyTraceChannel);
    EnemyObjects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    EnemyObjects.AddObjectTypesToQuery(ECC_Pawn);

    TArray<FHitResult> EnemyHits;
    GetWorld()->SweepMultiByObjectType(EnemyHits,OldLocation,OldLocation + SweepDelta,UpdatedComponent->GetComponentQuat(),EnemyObjects,CapShape,TraceParams);
    bool bHitEnemy = false;
    for (const FHitResult& Hit : EnemyHits)
    {
        if (Cast<APawn>(Hit.GetActor()))
        {
            EnemyHit = Hit;
            bHitEnemy = true;
            break;
        }
    }

    if (bHitEnemy)
    {
        const float DistanceToEnemy = EnemyHit.bStartPenetrating ? 0.0f : EnemyHit.Time * SweepDelta.Size();
        MoveDelta = MeleeDir * FMath::Clamp(DistanceToEnemy - HitReach, 0.0f, MoveDelta.Size());
    }


    FHitResult WallHit(1.f);
    FMovementRecord MoveRecord;
    MoveRecord.SetDeltaSeconds(DeltaSeconds);

    UMovementUtils::TrySafeMoveAndSlideUpdatedComponent(Params.MovingComps,MoveDelta,NewRot,true,WallHit,ETeleportType::None,MoveRecord,true);


    FVector FinalVelocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaSeconds;

    bool bLungeEnded = false;
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
        bLungeEnded = true;
    }
    else if (OutputMeleeState.ElapsedTimeMs >= (MaxDuration * 1000.0f))
    {
        FinalVelocity =FVector::ZeroVector;
        OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
        bLungeEnded = true;
    }

    // The server never resimulates, so there this fires exactly once per lunge.
    if (bLungeEnded && !Params.TimeStep.bIsResimulating)
    {
        const FVector LungeStart = StartingMeleeState ? StartingMeleeState->StartLocation : OldLocation;
        OnMeleeLungeEnded.Broadcast(bHitEnemy ? EnemyHit.GetActor() : nullptr, LungeStart);
    }
    

    OutputSyncState.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(),NewRot.Rotator(),FinalVelocity,FVector::ZeroVector,nullptr);

    UpdatedComponent->ComponentVelocity = FinalVelocity;
}