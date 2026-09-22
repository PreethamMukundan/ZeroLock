// Copyright Preetham Mukundan (C) 2026


#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"

#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Zero_BasePlayerController.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Weapon/Projectile.h"
#include "ZeroLock/ZeroLock.h"

UZL_Task_SpawnPredictedProjectile* UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(
	UGameplayAbility* OwningAbility, TSubclassOf<AProjectile> ProjectileClass, FVector SpawnLocation,
	FRotator SpawnRotation)
{
	if (!ensureAlwaysMsgf(OwningAbility->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted, TEXT("SpawnPredictedProjectile ability task activated in ability (%s), which does not have a net execution policy of Local Predicted. SpawnPredictedProjectile should only be used in predicted abilities. Use AbilityTask_SpawnActor otherwise."), *GetNameSafe(OwningAbility)))
	{
		return nullptr;
	}
 
	if (!ensureAlwaysMsgf(IsValid(ProjectileClass), TEXT("SpawnPredictedProjectile ability task activated in ability (%s) without a valid projectile class set."), *GetNameSafe(OwningAbility)))
	{
		return nullptr;
	}
 
	UZL_Task_SpawnPredictedProjectile* Task = NewAbilityTask<UZL_Task_SpawnPredictedProjectile>(OwningAbility);
	Task->ProjectileClass = ProjectileClass;
	Task->SpawnLocation = SpawnLocation;
	Task->SpawnRotation = SpawnRotation;
	return Task;
}

void UZL_Task_SpawnPredictedProjectile::Activate()
{
	Super::Activate();
	
		// On the client, listen for if this task is rejected. If it is, we need to destroy our fake projectile.
	if (IsPredictingClient())
	{
		GetActivationPredictionKey().NewRejectedDelegate().BindUObject(this, &UZL_Task_SpawnPredictedProjectile::OnTaskRejected);
	}
 
	if (Ability && Ability->GetCurrentActorInfo())
	{
		if (AZero_BasePlayerController* ZeroPc = Ability->GetCurrentActorInfo()->PlayerController.IsValid() ? Cast<AZero_BasePlayerController>(Ability->GetCurrentActorInfo()->PlayerController.Get()) : nullptr)
		{
			const float ForwardPredictionTime = ZeroPc->GetForwardPredictionTime();
			const bool bIsNetAuthority = Ability->GetCurrentActorInfo()->IsNetAuthority();
			// The server can spawn the actor without waiting for the client's data if it's a listen server or a standalone client.
			const bool bShouldUseServerInfo = IsLocallyControlled();
 
			// On the server (if it's not a listen server), wait for the client to send us the projectile's spawn info.
			if (bIsNetAuthority && !bShouldUseServerInfo)
			{
				const FGameplayAbilitySpecHandle& SpecHandle = GetAbilitySpecHandle();
				const FPredictionKey& ActivationPredictionKey = GetActivationPredictionKey();
 
				AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UZL_Task_SpawnPredictedProjectile::OnSpawnDataReplicated);
				AbilitySystemComponent->AbilityTargetDataCancelledDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UZL_Task_SpawnPredictedProjectile::OnSpawnDataCancelled);
 
				// Check if the client already sent the data.
				AbilitySystemComponent->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey);
 
				// Kill the ability if we never receive the data.
				SetWaitingOnRemotePlayerData();
 
				return;
			}
 
			/* On the client, if our ping is too high to forward-predict, delay spawning the projectile so we don't
			 * forward-predict further than MaxPredictionPing. */
			if (!bIsNetAuthority)
			{
				// We wait just enough to spawn the projectile with the maximum possible forward-prediction. 
				float SleepTime = ZeroPc->GetProjectileSleepTime();
				if (SleepTime > 0.0f)
				{
					if (!GetWorld()->GetTimerManager().IsTimerActive(SpawnDelayedFakeProjHandle))
					{
						// Set a timer to spawn the predicted projectile after a delay.
						DelayedProjectileInfo.ProjectileClass = ProjectileClass;
						DelayedProjectileInfo.SpawnLocation = SpawnLocation;
						DelayedProjectileInfo.SpawnRotation = SpawnRotation;
						DelayedProjectileInfo.ZeroPC = ZeroPc;
						DelayedProjectileInfo.ProjectileId = ZeroPc->GenerateNewFakeProjectileId();
						GetWorld()->GetTimerManager().SetTimer(SpawnDelayedFakeProjHandle, this, &UZL_Task_SpawnPredictedProjectile::SpawnDelayedFakeProjectile, SleepTime, false);
					}
 
					return;
				}
			}
 
			/* On the client, if our ping is low enough to forward-predict (or we're on LAN), immediately spawn and 
			 * initialize the fake projectile. */
			if (!bIsNetAuthority)
			{
				const uint32 FakeProjectileId = ZeroPc->GenerateNewFakeProjectileId();
				if (AProjectile* NewProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, GenerateSpawnParamsForFake(FakeProjectileId)))
				{
					
					// Send the spawn info to the server so it can spawn the authoritative projectile.
					SendSpawnDataToServer(SpawnLocation, SpawnRotation, FakeProjectileId);
 
					// Cache the projectile in case the server rejects this task, and we have to destroy it.
					SpawnedFakeProj = NewProjectile;
 
					if (ShouldBroadcastAbilityTaskDelegates())
					{
						Success.Broadcast(NewProjectile);
					}
 
					// We don't end the task here because we need to keep listening for possible rejection.
 
					return;
				}
			}
			/* On listen servers or in standalone, spawn the authoritative projectile. No prediction is needed in this
			 * case. Remote servers don't spawn authoritative actors until OnTargetDataReplicated. */
			else if (bIsNetAuthority && bShouldUseServerInfo)
			{
				if (AProjectile* NewProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, GenerateSpawnParams()))
				{
				
					if (ShouldBroadcastAbilityTaskDelegates())
					{
						Success.Broadcast(NewProjectile);
					}
 
					EndTask();
 
					return;
				}
			}
		}
	}
 
	// Cancel the task on the server if the client failed.
	CancelServerSpawn();
 
	// Failed to spawn. Ability should usually be cancelled at this point.
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FailedToSpawn.Broadcast(nullptr);
	}
 
	EndTask();
}

void UZL_Task_SpawnPredictedProjectile::SpawnDelayedFakeProjectile()
{
	if (Ability && Ability->GetCurrentActorInfo() && DelayedProjectileInfo.ZeroPC.IsValid())
	{
		if (AProjectile* NewProjectile = GetWorld()->SpawnActor<AProjectile>(DelayedProjectileInfo.ProjectileClass, DelayedProjectileInfo.SpawnLocation, DelayedProjectileInfo.SpawnRotation, GenerateSpawnParamsForFake(DelayedProjectileInfo.ProjectileId)))
		{
		
			// Send the spawn info to the server so it can spawn the authoritative projectile.
			SendSpawnDataToServer(DelayedProjectileInfo.SpawnLocation, DelayedProjectileInfo.SpawnRotation, DelayedProjectileInfo.ProjectileId);
 
			SpawnedFakeProj = NewProjectile;
 
			if (ShouldBroadcastAbilityTaskDelegates())
			{
				Success.Broadcast(NewProjectile);
			}
 
			// We don't end the task here because we need to keep listening for possible rejection.
 
			return;
		}
	}
 
	// Cancel the task on the server if the client failed.
	CancelServerSpawn();
 
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FailedToSpawn.Broadcast(nullptr);
	}
 
	EndTask();
}

void UZL_Task_SpawnPredictedProjectile::SendSpawnDataToServer(const FVector& InLocation, const FRotator& InRotation,
	uint32 InProjectileId)
{
	const bool bGenerateNewKey = !AbilitySystemComponent->ScopedPredictionKey.IsValidForMorePrediction();
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get(), bGenerateNewKey);
	FGameplayAbilityTargetDataHandle Handle = FGameplayAbilityTargetData_ProjectileSpawnInfo::MakeProjectileSpawnInfoTargetData(InLocation, InRotation, InProjectileId);
	AbilitySystemComponent->CallServerSetReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey(), Handle, FGameplayTag(), AbilitySystemComponent->ScopedPredictionKey);
}

void UZL_Task_SpawnPredictedProjectile::CancelServerSpawn()
{
	const bool bGenerateNewKey = !AbilitySystemComponent->ScopedPredictionKey.IsValidForMorePrediction();
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get(), bGenerateNewKey);
	AbilitySystemComponent->ServerSetReplicatedTargetDataCancelled(GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
}

void UZL_Task_SpawnPredictedProjectile::OnSpawnDataReplicated(const FGameplayAbilityTargetDataHandle& Data,
	FGameplayTag Activation)
{
	// Copy the target data before we consume it.
	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
 
	// Consume the client's data. Ensures each server task only spawns one projectile for each client task.
	if (!Cast<UBaseCharAbilitySystemComponent>(AbilitySystemComponent)->TryConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey()))
	{
		return;
	}
 
	if (TargetData)
	{
		if (const FGameplayAbilityTargetData_ProjectileSpawnInfo* SpawnInfo = static_cast<const FGameplayAbilityTargetData_ProjectileSpawnInfo*>(TargetData))
		{
			AZero_BasePlayerController* ZeroPc = Ability->GetCurrentActorInfo()->PlayerController.IsValid() ? Cast<AZero_BasePlayerController>(Ability->GetCurrentActorInfo()->PlayerController.Get()) : nullptr;
			const float ForwardPredictionTime = ZeroPc->GetForwardPredictionTime();
 
			if (AProjectile* NewProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnInfo->SpawnLocation, SpawnInfo->SpawnRotation, GenerateSpawnParamsForAuth(SpawnInfo->ProjectileId)))
			{
				// Note that there will be a discrepancy between the server's perceived ping and the client's.
				
				if (NewProjectile->ProjectileMovement)
				{
					// Tick the actor (e.g. animations, VFX).
					if (NewProjectile->PrimaryActorTick.IsTickFunctionEnabled())
					{
						NewProjectile->TickActor(ForwardPredictionTime * NewProjectile->CustomTimeDilation, LEVELTICK_All, NewProjectile->PrimaryActorTick);
					}
 
					// Tick the movement component (to actually move the projectile).
					NewProjectile->ProjectileMovement->TickComponent(ForwardPredictionTime * NewProjectile->CustomTimeDilation, LEVELTICK_All, nullptr);
					if (NewProjectile->GetLifeSpan() > 0.0f)
					{
						/* Since we're fast-forwarding this actor, we need to subtract the forward prediction time from
						 * its lifespan. Clamp at 0.2 so we have enough time to replicate. */
						NewProjectile->SetLifeSpan(FMath::Max(0.2f, NewProjectile->GetLifeSpan() - ForwardPredictionTime));
					}
				}
 
				if (ShouldBroadcastAbilityTaskDelegates())
				{
					Success.Broadcast(NewProjectile);
				}
 
				EndTask();
 
				return;
			}
		}
	}
 
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FailedToSpawn.Broadcast(nullptr);
	}
 
	EndTask();
}

void UZL_Task_SpawnPredictedProjectile::OnSpawnDataCancelled()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FailedToSpawn.Broadcast(nullptr);
	}
 
	EndTask();
}

void UZL_Task_SpawnPredictedProjectile::OnTaskRejected()
{
	
	AZero_BasePlayerController* ZeroPc = (Ability && Ability->GetCurrentActorInfo()) ? Cast<AZero_BasePlayerController>(Ability->GetCurrentActorInfo()->PlayerController) : nullptr;
 
	// If we've spawned a fake projectile on the client, destroy it.
	if (SpawnedFakeProj.IsValid())
	{
		// The fake projectile will still be lingering on the PC's list of unlinked projectiles; we need to remove it.
		if (ZeroPc)
		{
			if (const uint32* Key = ZeroPc->FakeProjectiles.FindKey(SpawnedFakeProj.Get()))
			{
				UE_LOG(LogTemp, Error, TEXT("Removed %i"), *Key);
				ZeroPc->FakeProjectiles.Remove(*Key);
			}
		}
 
		SpawnedFakeProj.Get()->Destroy();
	}
 
	// If we didn't spawn the fake projectile yet (because we're waiting for a delayed spawn), cancel it.
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
}

FActorSpawnParameters UZL_Task_SpawnPredictedProjectile::GenerateSpawnParams() const
{
	FActorSpawnParameters Params;
	Params.Instigator = Ability->GetCurrentActorInfo()->AvatarActor.IsValid() ? Cast<APawn>(Ability->GetCurrentActorInfo()->AvatarActor) : nullptr;
	Params.Owner = AbilitySystemComponent->GetOwnerActor();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return Params;
}

FActorSpawnParameters UZL_Task_SpawnPredictedProjectile::GenerateSpawnParamsForFake(const uint32 ProjectileId) const
{
	// Fake projectiles need to be marked as fake before BeginPlay.
	FActorSpawnParameters Params = GenerateSpawnParams();
	AZero_BasePlayerController* ZeroPc = Cast<AZero_BasePlayerController>(Ability->GetCurrentActorInfo()->PlayerController);
	Params.CustomPreSpawnInitialization = [ProjectileId, ZeroPc](AActor* Actor)
	{
		if (AProjectile* Projectile = Cast<AProjectile>(Actor))
		{
			Projectile->InitProjectileId(ProjectileId);
			Projectile->InitFakeProjectile(ZeroPc, ProjectileId);
		}
	};
	return Params;
}

FActorSpawnParameters UZL_Task_SpawnPredictedProjectile::GenerateSpawnParamsForAuth(const uint32 ProjectileId) const
{
	// Authoritative projectiles need to initialize their ID before BeginPlay.
	FActorSpawnParameters Params = GenerateSpawnParams();
	Params.CustomPreSpawnInitialization = [ProjectileId](AActor* Actor)
	{
		if (AProjectile* Projectile = Cast<AProjectile>(Actor))
		{
			Projectile->InitProjectileId(ProjectileId);
		}
	};
	return Params;
}
