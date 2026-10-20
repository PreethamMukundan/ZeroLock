//Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_BasePlayAnimation_AndDo.h"

#include "AbilitySystemComponent.h"
#include "GAS/Tasks/GAST_PlayMontageAndWaitForEvent.h"

UZL_BasePlayAnimation_AndDo::UZL_BasePlayAnimation_AndDo()
{
}

void UZL_BasePlayAnimation_AndDo::OnCanelled(FGameplayTag EventTag, FGameplayEventData EventData)
{
	OnAnimationCancelled();
}

void UZL_BasePlayAnimation_AndDo::OnCompleted(FGameplayTag EventTag, FGameplayEventData EventData)
{
	OnAnimationCompleted();
}


void UZL_BasePlayAnimation_AndDo::OnReceived(FGameplayTag EventTag, FGameplayEventData EventData)
{
	OnAnimationPointTrigger();
}

void UZL_BasePlayAnimation_AndDo::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                  const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                  const FGameplayEventData* TriggerEventData)
{
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!EventTagToWait.IsValid() || !MontageToPlay)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
    
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
    
	PlayMontageTask = UGAST_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(this, FName("MontagetoPlay"), MontageToPlay, EventTagToWait);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UZL_BasePlayAnimation_AndDo::OnCanelled);
	PlayMontageTask->OnCompleted.AddDynamic(this, &UZL_BasePlayAnimation_AndDo::OnCompleted);
	PlayMontageTask->EventReceived.AddDynamic(this, &UZL_BasePlayAnimation_AndDo::OnReceived);
	PlayMontageTask->ReadyForActivation();
}

void UZL_BasePlayAnimation_AndDo::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// The predicting client reaches the anim notify / timers before the server. Replicating its end cancels the server's
	// instance before the server gets to the same point, where the damage and effects are applied, so these abilities only
	// worked when the host cast them. The server ends its own instance and replicates that down instead.
	// Real cancels still reach the server: CancelAbility replicates on its own and calls EndAbility without replication.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZL_BasePlayAnimation_AndDo::OnAnimationPointTrigger()
{
	ZLOG("event");
	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true, false);
}

void UZL_BasePlayAnimation_AndDo::OnAnimationCancelled()
{
	ZLOG("Cancelled");
	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true, true);
}

void UZL_BasePlayAnimation_AndDo::OnAnimationCompleted()
{
	ZLOG("Completed");
	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true, false);

}
