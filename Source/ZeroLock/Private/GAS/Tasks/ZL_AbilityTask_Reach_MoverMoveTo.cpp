// Copyright Preetham Mukundan (C) 2026


#include "GAS/Tasks/ZL_AbilityTask_Reach_MoverMoveTo.h"

#include "Mover/ZeroMoverComponent.h"

UZL_AbilityTask_Reach_MoverMoveTo::UZL_AbilityTask_Reach_MoverMoveTo(const FObjectInitializer& ObjectInitializer)
{
	bTickingTask = true;
}

UZL_AbilityTask_Reach_MoverMoveTo* UZL_AbilityTask_Reach_MoverMoveTo::ApplyMoverReachMoveTo(
	UGameplayAbility* OwningAbility, FName TaskInstanceName, FVector StartLocation, FVector TargetLocation,
	float Duration, float DistDelta, float maxDuration, bool bStopAtEnd)
{
	UZL_AbilityTask_Reach_MoverMoveTo* MyTask = NewAbilityTask<UZL_AbilityTask_Reach_MoverMoveTo>(OwningAbility, TaskInstanceName);
	MyTask->StartLoc = StartLocation;
	MyTask->TargetLoc = TargetLocation;
	MyTask->MoveDuration = Duration;
	MyTask->MaxDuration = maxDuration;
	MyTask->FinishDistanceDelta = DistDelta;
	MyTask->bStopInEnd = bStopAtEnd;
	return MyTask;
}

void UZL_AbilityTask_Reach_MoverMoveTo::Activate()
{
	Super::Activate();

	if (AActor* Avatar = GetAvatarActor())
	{
		if (UZeroMoverComponent* MoverComp = Avatar->FindComponentByClass<UZeroMoverComponent>())
		{
			FVector Direction = (TargetLoc - StartLoc);
			FVector Velocity = Direction / MoveDuration;
            
			MoverComp->RequestSafeAbilityMove(Velocity, MoveDuration);

			GetWorld()->GetTimerManager().SetTimer(FinishTimerHandle, this, &UZL_AbilityTask_Reach_MoverMoveTo::MaxTImerReached, MaxDuration, false);
			return;
		}
	}
	OnMoveComplete(false);
}

void UZL_AbilityTask_Reach_MoverMoveTo::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (AActor* Avatar = GetAvatarActor())
	{
		if (UZeroMoverComponent* MoverComp = Avatar->FindComponentByClass<UZeroMoverComponent>())
		{
			if (MoverComp->GetMovementModeName() == DefaultModeNames::Walking)
			{
				OnMoveComplete(true);
				return;
			}

			
			if (FVector::Dist(Avatar->GetActorLocation(), TargetLoc) <= FinishDistanceDelta)
			{
				OnMoveComplete(true);
				return;
			}
		}
	}
}

void UZL_AbilityTask_Reach_MoverMoveTo::OnDestroy(bool bInOwnerFinished)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FinishTimerHandle);
	}
	Super::OnDestroy(bInOwnerFinished);
}

void UZL_AbilityTask_Reach_MoverMoveTo::OnMoveComplete(bool ReachedLocation)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast(ReachedLocation);
	}
	if (bStopInEnd)
	{
		if (AActor* Avatar = GetAvatarActor())
		{
			if (UZeroMoverComponent* MoverComp = Avatar->FindComponentByClass<UZeroMoverComponent>())
			{
				FVector Direction = FVector::ZeroVector;
				FVector Velocity = FVector::ZeroVector;
            
				MoverComp->RequestSafeAbilityMove(Velocity, MoveDuration);
			}
		}
	}
	
	EndTask();
}

void UZL_AbilityTask_Reach_MoverMoveTo::MaxTImerReached()
{
	OnMoveComplete(false);
}
