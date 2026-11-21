// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GAS/BasePassive_GameplayAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "Items/Zero_Item_data.h"
#include "ZL_KaiKoa_FrozenFist.generated.h"

class AZeroLockCharacter;

/**
 * Frost stacks on an enemy (one stack each). AggregateByTarget + ClearEntireStack, and every new stack restarts the
 * duration, so all of them go together. Frozen Fist sets the duration per spec (FrostDecayTime) and grants
 * Zerolock.DebuffStack.Frost through the spec, so the stacks show on the HUD (GAS.md "Debuff stacks").
 */
UCLASS()
class ZEROLOCK_API UZL_GE_KaiKoa_Frost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_KaiKoa_Frost();

	static int32 GetStacks(const UAbilitySystemComponent* TargetASC);
};

/**
 * Kai Koa's passive, Frozen Fist. Server only, always on (in the hero's DefaultAbilities). Each of his melee hits
 * (FrostTriggers: light and heavy melee) adds StacksPerHit frost to the enemy (FrostCue). Frost fades FrostDecayTime
 * seconds after the last new stack. At StacksToFreeze the frost clears and the enemy is frozen: stun + root for
 * FreezeDuration with a status timer and FrozenCue. A frozen enemy gets no new frost until the freeze ends.
 */
UCLASS()
class ZEROLOCK_API UZL_KaiKoa_FrozenFist : public UBasePassive_GameplayAbility
{
	GENERATED_BODY()

public:
	UZL_KaiKoa_FrozenFist();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Frozen Fist")
	TArray<EZL_ProcTrigger> FrostTriggers;

	UPROPERTY(EditDefaultsOnly, Category = "Frozen Fist", meta = (ClampMin = "1"))
	int32 StacksPerHit = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Frozen Fist", meta = (ClampMin = "1"))
	int32 StacksToFreeze = 3;

	/** Seconds without a new stack before all of them fade. */
	UPROPERTY(EditDefaultsOnly, Category = "Frozen Fist")
	float FrostDecayTime = 3.0f;

	/** Seconds before the same enemy can gain frost again, so one multi-hit strike counts once. */
	UPROPERTY(EditDefaultsOnly, Category = "Frozen Fist")
	float StackCooldownPerTarget = 0.2f;

	/** Stun + root length. */
	UPROPERTY(EditDefaultsOnly, Category = "Frozen Fist")
	FScalableFloat FreezeDuration = 1.5f;

	/** On the enemy for each stack below the freeze. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue FrostCue;

	/** On the enemy for the freeze. Its Duration should match FreezeDuration. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue FrozenCue;

private:
	void OnDamageDealt(AZeroLockCharacter* Villan, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource);

	FDelegateHandle DamageDealtHandle;
	TMap<TWeakObjectPtr<AZeroLockCharacter>, double> LastStackTime;
	TMap<TWeakObjectPtr<AZeroLockCharacter>, double> FrozenUntil;
};
