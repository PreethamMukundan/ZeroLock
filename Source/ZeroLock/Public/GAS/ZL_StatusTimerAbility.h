// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_StatusTimerAbility.generated.h"

class UZL_WaitDelay_Task;

/**
 * Shows a status (afflicted, slowed, ...) on the HUD of the hero it was applied to, as a progress bar that runs for the status's duration.
 * It runs on the victim, not the caster: the caster calls SendStatusTimer on the server, which triggers this through
 * Zerolock.Event.StatusTimer. The bar is labelled with the last part of the status tag (Zerolock.Status.Afflicted -> "Afflicted").
 * Different statuses get their own bars; sending a status that's already showing restarts its bar.
 * Every hero needs this in DefaultAbilities.
 */
UCLASS()
class ZEROLOCK_API UZL_StatusTimerAbility : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_StatusTimerAbility();

	/** Server only. Shows StatusTag on TargetASC's owner for Duration seconds. */
	static void SendStatusTimer(AActor* Instigator, UAbilitySystemComponent* TargetASC, FGameplayTag StatusTag, float Duration);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	FGameplayTag StatusTag;

	UPROPERTY()
	UZL_WaitDelay_Task* DurationTask;

	void EndOtherTimersForStatus();

	UFUNCTION()
	void OnDurationFinished();
};
