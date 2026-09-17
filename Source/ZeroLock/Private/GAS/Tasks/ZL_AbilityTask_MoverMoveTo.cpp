// Copyright Preetham Mukundan (C) 2026


#include "GAS/Tasks/ZL_AbilityTask_MoverMoveTo.h"

#include "TimerManager.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Engine/World.h"
#include "Mover/ZeroMoverComponent.h"

UZL_AbilityTask_MoverMoveTo::UZL_AbilityTask_MoverMoveTo(const FObjectInitializer& ObjectInitializer)
{
	bTickingTask = false;
}

UZL_AbilityTask_MoverMoveTo* UZL_AbilityTask_MoverMoveTo::ApplyMoverMoveTo(UGameplayAbility* OwningAbility,
	FName TaskInstanceName, FVector StartLocation, FVector TargetLocation, float Duration, bool bStopAtEnd)
{

	UZL_AbilityTask_MoverMoveTo* MyTask = NewAbilityTask<UZL_AbilityTask_MoverMoveTo>(OwningAbility, TaskInstanceName);
	MyTask->StartLoc = StartLocation;
	MyTask->TargetLoc = TargetLocation;
	MyTask->MoveDuration = Duration;
	MyTask->bStopInEnd = bStopAtEnd;
	return MyTask;
}

void UZL_AbilityTask_MoverMoveTo::Activate()
{
	Super::Activate();

	if (AActor* Avatar = GetAvatarActor())
	{
		if (UZeroMoverComponent* MoverComp = Avatar->FindComponentByClass<UZeroMoverComponent>())
		{
			FVector Direction = (TargetLoc - StartLoc);
			FVector Velocity = Direction / MoveDuration;
            
			MoverComp->RequestSafeAbilityMove(Velocity, MoveDuration);

			GetWorld()->GetTimerManager().SetTimer(FinishTimerHandle, this, &UZL_AbilityTask_MoverMoveTo::OnMoveComplete, MoveDuration, false);
			return;
		}
	}
	OnMoveComplete();
}

void UZL_AbilityTask_MoverMoveTo::OnDestroy(bool bInOwnerFinished)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FinishTimerHandle);
	}
	Super::OnDestroy(bInOwnerFinished);
}

void UZL_AbilityTask_MoverMoveTo::OnMoveComplete()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast();
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
