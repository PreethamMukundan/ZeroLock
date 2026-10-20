//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZeroBase_LightMelee.generated.h"

/**
 * Light melee (InputID LightMelee, IA_LightMelee). Plays a montage; on Event.Melee.Hit from a montage notify, or HitTime
 * seconds in if the montage has none, the server strikes in front of the hero with AZeroLockCharacter::PerformMeleeStrike
 * (same parry check as heavy melee: a parrying enemy stuns the attacker).
 * Montage: LightMeleeMontage here, else the hero's LightMeleeMontage, else its Deadlock anim set's.
 */
UCLASS()
class ZEROLOCK_API UZeroBase_LightMelee : public UBaseGameplayAbility
{
	GENERATED_BODY()
	UZeroBase_LightMelee();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData);
	UFUNCTION()
	void OnMontageCancelled(FGameplayTag EventTag, FGameplayEventData EventData);
	UFUNCTION()
	void OnHitEvent(FGameplayTag EventTag, FGameplayEventData EventData);
	UFUNCTION()
	void Strike();

	UAnimMontage* FindMontage() const;
	/** Only the server replicates the end: the client's montage can finish before the server's hit. */
	void FinishAbility(bool bWasCancelled);

	bool bStruck = false;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float StrikeDamage = 40.f;

	/** Reach in front of the hero, from the capsule edge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float StrikeRange = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float StrikeRadius = 50.f;

	/** Seconds into the montage the hit lands when the montage has no Event.Melee.Hit notify. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0"))
	float HitTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MontagePlayRate = 1.f;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Anim")
	UAnimMontage* LightMeleeMontage;

	/** Unused since the strike moved to AZeroLockCharacter::PerformMeleeStrike (kept so BP_LightMelee loads cleanly). */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> LightMeleeDamageEffect;

	/** Unused: a parried strike applies the attacker's ParriedStunEffect. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> ParryEffect;
};
