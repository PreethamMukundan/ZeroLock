// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GAS/BasePassive_GameplayAbility.h"
#include "ZeroLock/ZeroLock.h"
#include "ZL_SCombo_Combo.generated.h"

class AZeroLockCharacter;
class UAbilitySystemComponent;
enum class EZL_ProcTrigger : uint8;

namespace ZLSCombo
{
	/** Current combo stacks. Works on the owner's machine too: the effect replicates. */
	ZEROLOCK_API int32 GetComboStacks(UAbilitySystemComponent* ASC);

	/** The stack limit of UZL_GE_SCombo_Combo. */
	ZEROLOCK_API int32 GetMaxComboStacks();

	/** Live enemies within Radius of the hero, with no wall between them. Doesn't check Untouchable: Apply*Damage does. */
	ZEROLOCK_API void FindEnemiesInRadius(AZeroLockCharacter* Hero, float Radius, TArray<AZeroLockCharacter*>& OutVillans);
}

/** One stack per combo step. When the duration runs out one stack falls off and the timer restarts; a new stack restarts it too. */
UCLASS()
class ZEROLOCK_API UZL_GE_SCombo_Combo : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_SCombo_Combo();
};

/**
 * SCombo's passive. Landing damage from a different source than the last hit adds a combo stack. Sources: gun, melee, and the
 * Secondry, Ability 1 and Ability 2 slots. The ultimate and item damage don't count. Server only; its AbilityHUDWidget is the
 * combo bar, added on the owner's machine with the ability.
 */
UCLASS()
class ZEROLOCK_API UZL_SCombo_ComboPassive : public UBasePassive_GameplayAbility
{
	GENERATED_BODY()

public:
	UZL_SCombo_ComboPassive();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Seconds after a gain before one stack falls off, then between each stack falling off. */
	UPROPERTY(EditDefaultsOnly, Category = "Combo")
	FScalableFloat StackDecayTime = 4.0f;

private:
	/** The combo source of a hit, as an EGASAbilityInputID. None = doesn't count. */
	static EGASAbilityInputID GetComboSource(EZL_ProcTrigger HitTrigger, const UObject* DamageSource);

	void OnDamageDealt(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger HitTrigger, const UObject* DamageSource);

	EGASAbilityInputID LastComboSource = EGASAbilityInputID::None;
	FDelegateHandle DamageDealtHandle;
};
