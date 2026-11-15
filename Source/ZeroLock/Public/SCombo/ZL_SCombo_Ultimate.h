// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_SCombo_Ultimate.generated.h"

class UAnimMontage;
class UZL_WaitDelay_Task;

/**
 * SCombo's ultimate. Refused below MinComboToCast stacks. Spends every stack, then for ChannelDuration the hero walks slowed and
 * deals spirit damage around them every TickInterval: TickDamage × (1 + DamagePerStack × stacks spent). Zerolock.Status.Channeling
 * blocks every other ability, the weapon, melee and dash; jump (space) cancels it and doesn't jump.
 */
UCLASS()
class ZEROLOCK_API UZL_SCombo_Ultimate : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_SCombo_Ultimate();

	/** Also needs MinComboToCast combo stacks. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	int32 GetMinComboToCast() const { return MinComboToCast; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	int32 MinComboToCast = 5;

	/** Seconds the mode lasts. */
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	FScalableFloat ChannelDuration = 5.0f;

	/** Radius of the damage (cm), scaled by the hero's ability range. */
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	FScalableFloat Radius = 500.0f;

	/** Spirit damage per tick with no stacks. */
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	FScalableFloat TickDamage = 20.0f;

	/** Bonus per stack spent: 0.1 = +10%. */
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	FScalableFloat DamagePerStack = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Ultimate", meta = (ClampMin = 0.05))
	float TickInterval = 0.5f;

	/** Move speed multiplier during the mode: 0.6 = 40% slower. */
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate", meta = (ClampMin = 0.1, ClampMax = 1))
	float MoveSpeedMultiplier = 0.6f;

	/** Plays during the mode and stops with it. Optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	TObjectPtr<UAnimMontage> ChannelMontage;

	/** Draws the radius on each tick on the server. On until the ability has a real effect. */
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	bool bDrawUltimateDebug = true;

private:
	void DamageTick();
	void OnJumpPressed();

	UFUNCTION()
	void OnChannelFinished();

	bool bChannelActive = false;
	int32 StacksSpent = 0;
	FTimerHandle DamageTimer;
	FDelegateHandle JumpPressedHandle;
	FActiveGameplayEffectHandle SlowHandle;

	UPROPERTY()
	TObjectPtr<UZL_WaitDelay_Task> ChannelTask;
};
