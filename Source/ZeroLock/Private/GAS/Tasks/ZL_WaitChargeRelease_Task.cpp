// Copyright Preetham Mukundan (C) 2026


#include "GAS/Tasks/ZL_WaitChargeRelease_Task.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Mover/ZeroMoverComponent.h"


UZL_WaitChargeRelease_Task* UZL_WaitChargeRelease_Task::WaitChargeRelease(UGameplayAbility* OwningAbility,
                                                                          UAnimMontage* InChargeMontage, float MaxDuration, float PerfectMin, float PerfectMax)
{
	UZL_WaitChargeRelease_Task* MyObj = NewAbilityTask<UZL_WaitChargeRelease_Task>(OwningAbility);
	MyObj->MaxChargeTime = MaxDuration;
	MyObj->PerfectWindowMin = PerfectMin;
	MyObj->PerfectWindowMax = PerfectMax;
	MyObj->bTickingTask = true;
	MyObj->ElapsedTime = 0.0f;
	MyObj->bTestInitialState =true;
	MyObj->ChargeMontage = InChargeMontage;
	return MyObj;
}

void UZL_WaitChargeRelease_Task::Activate()
{
	if (AActor* Avatar = GetAvatarActor())
	{
		if (UZeroMoverComponent* MoverComp = Avatar->FindComponentByClass<UZeroMoverComponent>())
		{
			MoverComp->QueueNextMode("Locked",true);
		}
	}
	ElapsedTime= 0;
	OnInit.Broadcast(MaxChargeTime,PerfectWindowMin,PerfectWindowMax);
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC && Ability)
	{
		if (ChargeMontage)
		{
			float Duration = ASC->PlayMontage(Ability,Ability->GetCurrentActivationInfo(),ChargeMontage,1);
			
		}
		
		if (bTestInitialState && IsLocallyControlled())
		{
			FGameplayAbilitySpec *Spec = Ability->GetCurrentAbilitySpec();
			if (Spec && !Spec->InputPressed)
			{
				OnInputReleased();
				return;
			}
		}

		ReleaseDelegateHandle = ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).AddUObject(this, &UZL_WaitChargeRelease_Task::OnInputReleased);
		if (IsForRemoteClient())
		{
			if (!ASC->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()))
			{
				SetWaitingOnRemotePlayerData();
			}
		}
	}
}

void UZL_WaitChargeRelease_Task::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	ElapsedTime += DeltaTime;

	float Progress = FMath::Clamp(ElapsedTime / MaxChargeTime, 0.f, 1.0f);
	bool bInPerfectWindow = (ElapsedTime >= PerfectWindowMin && ElapsedTime <= PerfectWindowMax);

	OnProgressUpdate.Broadcast(Progress, bInPerfectWindow, ElapsedTime);
	
	if (ElapsedTime >= MaxChargeTime)
	{
		OnInputReleased();
	}
}

void UZL_WaitChargeRelease_Task::OnDestroy(bool bInOwnerFinished)
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->CurrentMontageStop(-1);
	}
	if (AActor* Avatar = GetAvatarActor())
	{
		if (UZeroMoverComponent* MoverComp = Avatar->FindComponentByClass<UZeroMoverComponent>())
		{
			MoverComp->QueueNextMode("Falling",true);
		}
	}
	Super::OnDestroy(bInOwnerFinished);
}

void UZL_WaitChargeRelease_Task::OnInputReleased()
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased,GetAbilitySpecHandle(),GetActivationPredictionKey()).Remove(ReleaseDelegateHandle);
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!Ability || !ASC)
	{
		return;
	}

	ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(ReleaseDelegateHandle);

	FScopedPredictionWindow ScopedPrediction(ASC, IsPredictingClient());

	if (IsPredictingClient())
	{
		ASC->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey(), ASC->ScopedPredictionKey);
	}
	else
	{
		ASC->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey());
	}
	
	bool bWasPerfect = (ElapsedTime >= PerfectWindowMin && ElapsedTime <= PerfectWindowMax);
    
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnEnd.Broadcast(bWasPerfect);
		OnReleased.Broadcast(ElapsedTime, bWasPerfect);
	}
    
	EndTask();
}
