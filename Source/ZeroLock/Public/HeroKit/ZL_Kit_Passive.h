// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitTypes.h"
#include "ZL_Kit_Passive.generated.h"

class UZero_Item_data;

/**
 * Hero kit passive (put it in the hero BP's DefaultAbilities). Server only, always on while alive:
 *   Procs: item-style procs (FZL_ItemProc, run by the hero's UZL_ItemProcComponent): on-hit chances, stacking buffs...
 *   PermanentStats / PermanentTags while alive.
 *   On a kill (Event.Kill), OnKillChance percent: OnKillPayload on the caster.
 * Subclass it for hero-specific passives (OnPassiveStarted / OnPassiveEnded / OnKill).
 */
UCLASS()
class ZEROLOCK_API UZL_Kit_Passive : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Kit_Passive();

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	TArray<FZL_ItemProc> Procs;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	TArray<FZL_ItemStatValue> PermanentStats;

	/** Loose tags on the server while the passive runs (e.g. Zerolock.Status.IgnoreBarrier). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FGameplayTagContainer PermanentTags;

	/** On whoever the hero damages (OnHitTriggers; empty = bullets and melee), OnHitChance percent (0 = always),
	 *  at most once per OnHitCooldown seconds. Unlike Procs, it carries a payload (with a HitCue). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|On Hit")
	FZL_KitPayload OnHitPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|On Hit")
	float OnHitChance = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|On Hit")
	float OnHitCooldown = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|On Hit")
	TArray<EZL_ProcTrigger> OnHitTriggers;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload OnKillPayload;

	/** Percent. 0 = always. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float OnKillChance = 0.0f;

	virtual void OnPassiveStarted() {}
	virtual void OnPassiveEnded() {}
	virtual void OnKill(AActor* Victim) {}

	AZeroLockCharacter* GetHero() const;

private:
	UFUNCTION()
	void HandleKill(FGameplayEventData Payload);

	void HandleDamageDealt(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource);

	FDelegateHandle DamageDealtHandle;
	double NextOnHitTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UZero_Item_data> ProcItem;

	FActiveGameplayEffectHandle StatsHandle;
	bool bEnding = false;
	bool bTagsAdded = false;
};
