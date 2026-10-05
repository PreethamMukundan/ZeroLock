// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_StatusTimerAbility.h"

#include "AbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"

UZL_StatusTimerAbility::UZL_StatusTimerAbility()
{
	// The server gets the event, then tells the owning client to activate too, since that's where the HUD is.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	// One instance per status, so several bars can run at once.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

	// A status, not something the victim casts: it must show up whatever they're doing, and never block their abilities.
	ActivationBlockedTags.Reset();
	BlockAbilitiesWithTag.Reset();
	CooldownGameplayEffectClass = nullptr;

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ZerolockGameplayTagsForBinding::TAG_EVENT_STATUS_TIMER;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UZL_StatusTimerAbility::SendStatusTimer(AActor* Instigator, UAbilitySystemComponent* TargetASC, FGameplayTag StatusTag, float Duration)
{
	if (!TargetASC || !StatusTag.IsValid() || Duration <= 0.0f)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.Instigator = Instigator;
	Payload.Target = TargetASC->GetAvatarActor();
	Payload.EventMagnitude = Duration;
	Payload.InstigatorTags.AddTag(StatusTag);
	TargetASC->HandleGameplayEvent(ZerolockGameplayTagsForBinding::TAG_EVENT_STATUS_TIMER, &Payload);
}

void UZL_StatusTimerAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                             const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const float Duration = TriggerEventData ? TriggerEventData->EventMagnitude : 0.0f;
	StatusTag = TriggerEventData ? TriggerEventData->InstigatorTags.First() : FGameplayTag();
	if (Duration <= 0.0f || !StatusTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	EndOtherTimersForStatus();

	// The bar's label: Zerolock.Status.Afflicted -> "Afflicted".
	FString Label = StatusTag.GetTagName().ToString();
	Label.Split(TEXT("."), nullptr, &Label, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	AbilityName = Label;

	DurationTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, Duration);
	DurationTask->OnProgress.AddDynamic(this, &UZL_StatusTimerAbility::UpdateProgressionTimer);
	DurationTask->OnStarted.AddDynamic(this, &UZL_StatusTimerAbility::StartProgressionTimer);
	DurationTask->OnEnd.AddDynamic(this, &UZL_StatusTimerAbility::StopProgressionTimer);
	DurationTask->OnFinished.AddDynamic(this, &UZL_StatusTimerAbility::OnDurationFinished);
	DurationTask->ReadyForActivation();
}

void UZL_StatusTimerAbility::EndOtherTimersForStatus()
{
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!Spec)
	{
		return;
	}

	for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
	{
		UZL_StatusTimerAbility* Other = Cast<UZL_StatusTimerAbility>(Instance);
		if (Other && Other != this && Other->IsActive() && Other->StatusTag == StatusTag)
		{
			Other->OnDurationFinished();
		}
	}
}

void UZL_StatusTimerAbility::OnDurationFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
