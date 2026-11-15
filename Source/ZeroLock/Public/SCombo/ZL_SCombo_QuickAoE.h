// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_SCombo_QuickAoE.generated.h"

class UAnimMontage;

/**
 * SCombo's third ability. Untouchable for UntouchableTime while AoEMontage plays; AoEDelay in, spirit damage to every enemy
 * around the hero. Runs on timers rather than a montage notify, so it works before the hero has real animations.
 */
UCLASS()
class ZEROLOCK_API UZL_SCombo_QuickAoE : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_SCombo_QuickAoE();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Radius of the AoE (cm), scaled by the hero's ability range. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickAoE")
	FScalableFloat AoERadius = 400.0f;

	/** Spirit damage to each enemy in the radius. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickAoE")
	FScalableFloat AoEDamage = 80.0f;

	/** Seconds the hero stays Untouchable, which is also how long the ability lasts. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickAoE")
	float UntouchableTime = 0.5f;

	/** Seconds from the press to the damage. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickAoE")
	float AoEDelay = 0.2f;

	/** Optional. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickAoE")
	TObjectPtr<UAnimMontage> AoEMontage;

	/** Draws the radius on the server. On until the ability has a real effect. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickAoE")
	bool bDrawAoEDebug = true;

private:
	UFUNCTION()
	void OnAoEDelayFinished();

	UFUNCTION()
	void OnUntouchableFinished();
};
