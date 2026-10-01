// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_GA_PullToAbility.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_GA_PullToAbility : public UBaseGameplayAbility
{
	GENERATED_BODY()
	
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
