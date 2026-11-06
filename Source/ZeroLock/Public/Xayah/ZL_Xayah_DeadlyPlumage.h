// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Xayah_DeadlyPlumage.generated.h"

class UZL_WaitDelay_Task;

/** Multiplies FireRate (seconds between shots) by SetByCaller TAG_SETBYCALLER_FIRE_RATE_MULTIPLIER. Duration set per spec. */
UCLASS()
class ZEROLOCK_API UZL_GE_Xayah_Plumage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_Xayah_Plumage();
};

/** Slot 2. Her weapon fires faster for BuffDuration. The effect is predicted, so her own gun speeds up at once. */
UCLASS()
class ZEROLOCK_API UZL_Xayah_DeadlyPlumage : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Xayah_DeadlyPlumage();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 0.5 = 50% more shots per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat FireRateBonus = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat BuffDuration = 5.0f;

	UPROPERTY()
	TObjectPtr<UZL_WaitDelay_Task> BuffTask;

	UFUNCTION()
	void OnBuffFinished();
};
