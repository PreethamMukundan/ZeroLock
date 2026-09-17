// Copyright Preetham Mukundan (C) 2026


#include "Mover/Transitions/ZeroZiplineTransition.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "MoverComponent.h"
#include "ColorManagement/TransferFunctions.h"
#include "Components/SplineComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Movement/Zero_ZiplineActor.h"
#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroZiplineMode.h"

FTransitionEvalResult UZeroZiplineTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
	UMoverComponent* MoverComp = Params.MovingComps.MoverComponent.Get();
	AActor* OwnerActor = MoverComp->GetOwner();
	
	if (Params.StartState.SyncState.MovementMode == ZipliningModeName) return EvalResult;

	if (const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>())
	{
		if (ZeroInputs->bWantsToZipline) 
		{
			FVector CamLocation = OwnerActor->GetActorLocation();
			FVector CamForward = ZeroInputs->LookDir.Vector();

			FCollisionShape ZipCap = FCollisionShape::MakeSphere(ZiplineCheckSphereRadius);
			FVector TraceStart = CamLocation + (CamForward * ZiplineCheckSphereRadius);
			FVector TraceEnd = CamLocation + (CamForward * ZiplineCheckMaxDistance);
            
			FHitResult ZipHit;
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ZiplineSweep), true, OwnerActor);
          
			if (GetWorld()->SweepSingleByObjectType(ZipHit, TraceStart, TraceEnd, ZeroInputs->LookDir.Quaternion(), ECC_Vehicle, ZipCap, TraceParams))
			{
				if (Cast<AZero_ZiplineActor>(ZipHit.GetActor()))
				{
					EvalResult.NextMode = ZipliningModeName;
				}
			}
		}
	}
	return EvalResult;
}


FTransitionEvalResult UZeroZiplineEndTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
	
	if (const FZeroMovementInputs* ZeroInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FZeroMovementInputs>())
	{

		if (ZeroInputs->bCustomJumpJustPressed || ZeroInputs->bWantsToCrouch) 
		{
			EvalResult.NextMode = AutoExitToMode;
		}
	}

	return EvalResult;
}
