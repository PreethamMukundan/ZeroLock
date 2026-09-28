// Copyright Preetham Mukundan (C) 2026

#include "GAS/Tasks/ZL_Task_WaitSlamLocation.h"

#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Task_WaitSlamLocation* UZL_Task_WaitSlamLocation::FindSlamLocation(UGameplayAbility* owningAbility,
                                                                       FName TaskInstanceName, AZeroLockCharacter* owningCharacter, float baseRadius, float heightMultiplier,
                                                                       float maxRadius)
{
	UZL_Task_WaitSlamLocation* NewTask = NewAbilityTask<UZL_Task_WaitSlamLocation>(owningAbility, TaskInstanceName);
	if (!NewTask)
	{
		return nullptr;
	}

	NewTask->TBaseRadius = baseRadius;
	NewTask->THeightMultiplier = heightMultiplier;
	NewTask->TMaxRadius = maxRadius;
	NewTask->OwningHero = owningCharacter;
	return NewTask;
}

void UZL_Task_WaitSlamLocation::Activate()
{
	Super::Activate();

	ZLOG_COLOR_TIME("TaskStart", FColor::Blue, 30);

	if (!Ability || !AbilitySystemComponent.IsValid())
	{
		EndTask();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		EndTask();
		return;
	}

	const bool bIsNetAuthority = ActorInfo->IsNetAuthority();
	const bool bShouldUseServerInfo = IsLocallyControlled();

	// On Dedicated Server (remote player), wait for client to replicate target data
	if (bIsNetAuthority && !bShouldUseServerInfo)
	{
		if (UBaseCharAbilitySystemComponent* BaseASC = Cast<UBaseCharAbilitySystemComponent>(AbilitySystemComponent.Get()))
		{
			// Data may have already arrived before we started waiting — consume it immediately.
			if (FVector* Cached = BaseASC->CachedSlamLocations.Find(GetActivationPredictionKey()))
			{
				FVector Location = *Cached;
				BaseASC->CachedSlamLocations.Remove(GetActivationPredictionKey());
				ReceiveLocationFromClient(Location);
				return;
			}

			BaseASC->RegisterPendingSlamTask(GetActivationPredictionKey(), this);
			SetWaitingOnRemotePlayerData();
		}
		else
		{
			EndTask();
		}
		return;
	}

	// On Autonomous Client, calculate location locally and send to server
	if (!bIsNetAuthority)
	{
		PerformTraceAndSendToServer();
		return;
	}

	// On Listen Server or Standalone host
	if (bIsNetAuthority && bShouldUseServerInfo)
	{
		FVector CalculatedLocation = FVector::ZeroVector;
		FHitResult TargetHit;
		if (OwningHero && GetLookAtLocation(OwningHero, TBaseRadius, THeightMultiplier, TMaxRadius, TargetHit))
		{
			CalculatedLocation = TargetHit.ImpactPoint;
		}

		UE_LOG(LogTemp, Warning, TEXT("[%s] Broadcasting OnLocationReceived, IsBound=%d"),
	AbilitySystemComponent.IsValid() && AbilitySystemComponent->GetOwnerActor() && AbilitySystemComponent->GetOwnerActor()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
	OnLocationReceived.IsBound());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
		DrawDebugSphere(GetWorld(), CalculatedLocation, 50, 50, FColor::Green, true);
			OnLocationReceived.Broadcast(CalculatedLocation);
		}

		EndTask();
		return;
	}

	EndTask();
}

void UZL_Task_WaitSlamLocation::ReceiveLocationFromClient(const FVector& Vector)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] Broadcasting OnLocationReceived, IsBound=%d"),
	AbilitySystemComponent.IsValid() && AbilitySystemComponent->GetOwnerActor() && AbilitySystemComponent->GetOwnerActor()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
	OnLocationReceived.IsBound());
	if (ShouldBroadcastAbilityTaskDelegates())
	{
	DrawDebugSphere(GetWorld(), Vector, 50, 50, FColor::Green, true);
		OnLocationReceived.Broadcast(Vector);
	}

	EndTask();
}

void UZL_Task_WaitSlamLocation::OnDestroy(bool bInOwnerFinished)
{
	if (UBaseCharAbilitySystemComponent* BaseASC = Cast<UBaseCharAbilitySystemComponent>(AbilitySystemComponent.Get()))
	{
		BaseASC->UnregisterPendingSlamTask(GetActivationPredictionKey());
	}
	Super::OnDestroy(bInOwnerFinished);
}

void UZL_Task_WaitSlamLocation::PerformTraceAndSendToServer()
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndTask();
		return;
	}

	FVector CalculatedLocation = FVector::ZeroVector;
	FHitResult TargetHit;
	if (OwningHero && GetLookAtLocation(OwningHero, TBaseRadius, THeightMultiplier, TMaxRadius, TargetHit))
	{
		CalculatedLocation = TargetHit.ImpactPoint;
	}

	if (UBaseCharAbilitySystemComponent* BaseASC = Cast<UBaseCharAbilitySystemComponent>(ASC))
	{
		BaseASC->ServerReportSlamLocation(GetActivationPredictionKey(), CalculatedLocation);
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] Broadcasting OnLocationReceived, IsBound=%d"),
	AbilitySystemComponent.IsValid() && AbilitySystemComponent->GetOwnerActor() && AbilitySystemComponent->GetOwnerActor()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
	OnLocationReceived.IsBound());
	if (ShouldBroadcastAbilityTaskDelegates())
	{
	DrawDebugSphere(GetWorld(), CalculatedLocation, 50, 50, FColor::Blue, true);
		OnLocationReceived.Broadcast(CalculatedLocation); // client's own predicted feedback
	}

	EndTask();
}

void UZL_Task_WaitSlamLocation::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data,
	FGameplayTag ActivationTag)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndTask();
		return;
	}
	
	// Consume client replicated target data safely
	if (UBaseCharAbilitySystemComponent* BaseASC = Cast<UBaseCharAbilitySystemComponent>(ASC))
	{
		if (!BaseASC->TryConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey()))
		{
			ZLOG_COLOR_TIME("ConsumeFailed",FColor::Black,30);
			return;
		}
	}
	
	else
	{
		ASC->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	}
	ZLOG_COLOR_TIME(FString::Printf(TEXT("Data.Num()=%d"), Data.Num()),FColor::Blue,40);
	if (Data.Num() > 0 && Data.Get(0))
	{
		FVector ReplicatedLocation = Data.Get(0)->GetEndPoint(); 
		UE_LOG(LogTemp, Warning, TEXT("[%s] Broadcasting OnLocationReceived, IsBound=%d"),
	AbilitySystemComponent.IsValid() && AbilitySystemComponent->GetOwnerActor() && AbilitySystemComponent->GetOwnerActor()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
	OnLocationReceived.IsBound());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
		DrawDebugSphere(GetWorld(), ReplicatedLocation, 50, 50, FColor::Green, true);
			OnLocationReceived.Broadcast(ReplicatedLocation);
		}
	}

	EndTask();
}

void UZL_Task_WaitSlamLocation::OnTargetDataCancelledCallback()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		ASC->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	EndTask();
}

bool UZL_Task_WaitSlamLocation::GetLookAtLocation(const AZeroLockCharacter* InActor, float BaseRadius,
	float HeightMultiplier, float MaxAllowedRadius, FHitResult& OutHit)
{
	if (!InActor || !InActor->GetWorld()) return false;

	UWorld* World = InActor->GetWorld();
	FVector ActorLoc = InActor->GetActorLocation();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(InActor);

	FHitResult HeightCheckHit;
	float Height = 0.f;
	if (World->LineTraceSingleByChannel(HeightCheckHit, ActorLoc - FVector(0, 0, 45), ActorLoc + (FVector::DownVector * 10000.f), ECC_Vehicle, Params))
	{
		Height = FVector::Dist(ActorLoc, HeightCheckHit.ImpactPoint);
	}

	float DynamicRadius = FMath::Clamp(BaseRadius + (Height * HeightMultiplier), BaseRadius, MaxAllowedRadius);
	FVector CenterBase = HeightCheckHit.bBlockingHit ? HeightCheckHit.ImpactPoint : ActorLoc;

	FVector Start = InActor->GetPawnViewLocation();
	FVector End = Start + (InActor->GetViewRotation().Vector() * 10000.f);
	FHitResult LookHit;

	if (World->LineTraceSingleByChannel(LookHit, Start, End, ECC_Visibility, Params))
	{
		FVector ToHitFlat = LookHit.ImpactPoint - CenterBase;
		ToHitFlat.Z = 0;

		if (ToHitFlat.Size() > DynamicRadius)
		{
			FVector ClampedPos = CenterBase + (ToHitFlat.GetSafeNormal() * DynamicRadius);
			return World->LineTraceSingleByChannel(OutHit, ClampedPos + FVector(0, 0, 500), ClampedPos + (FVector::DownVector * 1000.f), ECC_Visibility, Params);
		}
		//DrawDebugSphere(World, LookHit.ImpactPoint, 20, 20, FColor::Red, true);
		//DrawDebugLine(World, Start, LookHit.ImpactPoint, FColor::Green, true);
		OutHit = LookHit;
		return true;
	}

	return false;
}