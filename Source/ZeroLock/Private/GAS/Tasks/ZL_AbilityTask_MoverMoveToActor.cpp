// Copyright Preetham Mukundan (C) 2026


#include "GAS/Tasks/ZL_AbilityTask_MoverMoveToActor.h"

#include "Mover/ZeroMoverComponent.h"

UZL_AbilityTask_MoverMoveToActor::UZL_AbilityTask_MoverMoveToActor(const FObjectInitializer& ObjectInitializer)
{
	bTickingTask = true;
}

UZL_AbilityTask_MoverMoveToActor* UZL_AbilityTask_MoverMoveToActor::ApplyMoverMoveToActor(
	UGameplayAbility* OwningAbility, FName TaskInstanceName, AActor* TargetActor, float Duration, float AcceptanceRadius)
{
	UZL_AbilityTask_MoverMoveToActor* MyTask = NewAbilityTask<UZL_AbilityTask_MoverMoveToActor>(OwningAbility, TaskInstanceName);
	MyTask->TargetToTrack = TargetActor;
	MyTask->MoveDuration = Duration;
	MyTask->AcceptRadius = AcceptanceRadius;
	return MyTask;
}

void UZL_AbilityTask_MoverMoveToActor::Activate()
{
	Super::Activate();

	if (!TargetToTrack)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnTimeExpired.Broadcast();
		}
		EndTask();
		return;
	}

	if (AActor* Avatar = GetAvatarActor())
	{
		if (UZeroMoverComponent* MoverComp = Avatar->FindComponentByClass<UZeroMoverComponent>())
		{
			MoverComp->RequestSafeDynamicAbilityMove(TargetToTrack, MoveDuration);
			return;
		}
	}
	
	EndTask();
}

void UZL_AbilityTask_MoverMoveToActor::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!TargetToTrack)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnTimeExpired.Broadcast();
		}
		EndTask();
		return;
	}

	AActor* Avatar = GetAvatarActor();
	if (!Avatar)
	{
		EndTask();
		return;
	}

	float DistanceToTarget = FVector::Dist(Avatar->GetActorLocation(), TargetToTrack->GetActorLocation());

	if (DistanceToTarget <= AcceptRadius)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnTargetReached.Broadcast();
		}
		
		EndTask();
		return;
	}
	
	MoveDuration -= DeltaTime;
	if (MoveDuration <= 0.0f)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnTimeExpired.Broadcast(); 
		}
		EndTask();
	}
}
