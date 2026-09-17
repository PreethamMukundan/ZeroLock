// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroZiplineMode.h"

#include "DrawDebugHelpers.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MoveLibrary/MovementUtils.h"
#include "Movement/Zero_ZiplineActor.h"
#include "Mover/ZeroMovementData.h"


void FZeroZipliningState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("ZiplineActor: %s\n", *GetNameSafe(ZiplineActor));
	Out.Appendf("bMovingToEnd: %d\n", bMovingToEnd);
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
	bOutSuccess = true;
	return true;
}

bool FZeroZipliningState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FZeroZipliningState* AuthState = static_cast<const FZeroZipliningState*>(&AuthorityState);
	return (ZiplineActor != AuthState->ZiplineActor) || (bMovingToEnd != AuthState->bMovingToEnd);
}

void FZeroZipliningState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FZeroZipliningState* ToState = static_cast<const FZeroZipliningState*>(&To);
	ZiplineActor = ToState->ZiplineActor;
	bMovingToEnd = ToState->bMovingToEnd;
}

UScriptStruct* FZeroZipliningState::GetScriptStruct() const
{
	return FZeroZipliningState::StaticStruct();
}

UZeroZiplineMode::UZeroZiplineMode(const FObjectInitializer& ObjectInitializer)
{
}



void UZeroZiplineMode::SimulationTick_Implementation(const FSimulationTickParams& Params,
	FMoverTickEndData& OutputState)
{
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
    FZeroZipliningState& OutZipState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FZeroZipliningState>();
    
    // Check the Start State
    const FZeroZipliningState* StartZipState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FZeroZipliningState>();
    USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();

	
	const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>();
    if (!StartZipState || !StartZipState->ZiplineActor)
    {

        if (ZeroInputs)
        {
            FVector CamLocation = UpdatedComponent->GetComponentLocation();
            FVector CamForward = ZeroInputs->LookDir.Vector();
        
            FVector TraceStart = CamLocation + (CamForward * 1000.f); 
            FVector TraceEnd = CamLocation + (CamForward * 10000.f);  
            
            FHitResult ZipHit;
            FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ZiplineSweepInit), true, UpdatedComponent->GetOwner());
        
        	
            if (GetWorld()->SweepSingleByObjectType(ZipHit, TraceStart, TraceEnd, ZeroInputs->LookDir.Quaternion(), ECC_Vehicle, FCollisionShape::MakeSphere(1000.f), TraceParams))
            {
                // 1. Guard your cosmetics and debugs against rollbacks
                if (!Params.TimeStep.bIsResimulating)
                {
                    DrawDebugSphere(GetWorld(), ZipHit.ImpactPoint, 50.f, 16, FColor::Red, false, 5.0f);
                }
                
                if (AZero_ZiplineActor* ZP = Cast<AZero_ZiplineActor>(ZipHit.GetActor()))
                {
                    OutZipState.ZiplineActor = ZP;

                    USplineComponent* SplineComp = ZP->FindComponentByClass<USplineComponent>();
                    float DistanceAlongSpline = SplineComp->GetDistanceAlongSplineAtLocation(CamLocation, ESplineCoordinateSpace::World);
                    FVector TypSplineDir = SplineComp->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);
                
                    float Angle = UpdatedComponent->GetForwardVector() | TypSplineDir;
                    OutZipState.bMovingToEnd = (Angle > 0.0f);
                }
            }
        }

        if (!OutZipState.ZiplineActor)
        {
            OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
            return;
        }
    }
    else
    {
        OutZipState = *StartZipState;
    }

    USplineComponent* SplineComp = OutZipState.ZiplineActor->FindComponentByClass<USplineComponent>();
    if (!SplineComp)
    {
        OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
        return;
    }

    const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
    const FVector CurrentLoc = UpdatedComponent->GetComponentLocation();
    
    float SplineLength = SplineComp->GetSplineLength();
    float DistanceAlongSpline = SplineComp->GetDistanceAlongSplineAtLocation(CurrentLoc, ESplineCoordinateSpace::World);

    if ((OutZipState.bMovingToEnd && DistanceAlongSpline >= SplineLength) || (!OutZipState.bMovingToEnd && DistanceAlongSpline <= 0.0f))
    {
        OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
        OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
        return;
    }

	if (ZeroInputs->bCustomJumpJustPressed || ZeroInputs->bWantsToCrouch)
	{
		OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
		return;
	}

    float MoveDirection = OutZipState.bMovingToEnd ? 1.0f : -1.0f;
    float NextDistance = DistanceAlongSpline + (MoveDirection * ZiplineSpeed * DeltaSeconds);
    FVector TargetLocation = SplineComp->GetLocationAtDistanceAlongSpline(NextDistance, ESplineCoordinateSpace::World);
    

    FVector AdjustedDelta = TargetLocation - CurrentLoc;

    FVector SplineForward = SplineComp->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World).GetSafeNormal();
    SplineForward.Z = 0.0f;
    
    if (!OutZipState.bMovingToEnd)
    {
        SplineForward *= -1.0f;
    }
    FQuat NewRotation = FRotationMatrix::MakeFromXZ(SplineForward, FVector::UpVector).ToQuat();

    FMovementRecord MoveRecord;
    MoveRecord.SetDeltaSeconds(DeltaSeconds);
    FHitResult Hit(1.0f);

    UMovementUtils::TrySafeMoveUpdatedComponent(Params.MovingComps, AdjustedDelta, NewRotation, true, Hit, ETeleportType::None, MoveRecord);


    OutputState.SyncState.MovementMode = TEXT("Ziplining");

    FVector FinalVelocity = MoveRecord.GetRelevantVelocity();
    OutputSyncState.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentRotation(), FinalVelocity, FVector::ZeroVector);
    UpdatedComponent->ComponentVelocity = FinalVelocity;
}
