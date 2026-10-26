//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "Zero_BaseReloadAbility.generated.h"

class UAnimMontage;

/**
 * Reload: waits the reload time, then applies ReloadEffect (UCalc_Reload fills CurrentAmmo to MaxAmmo) on the server.
 * Holds TAG_WEAPON_RELOADING while it runs, which blocks primary fire. Not on a full clip. Plays ReloadMontage (else the
 * hero's DeadlockAnimSet->ReloadMontage) stretched to the reload time; GAS replicates it to the other players.
 */
UCLASS()
class ZEROLOCK_API UZero_BaseReloadAbility : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZero_BaseReloadAbility();

	UFUNCTION()
	void OnFinish();
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> ReloadEffect;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> ReloadingEffect;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Actually activate ability, do not call this directly. We'll call it from APAHeroCharacter::ActivateAbilitiesWithTags(). */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	class UAbilityTask_WaitDelay* TaskWaitDelay;

	/** Seconds per reload when the hero's ReloadTime attribute is 0. */
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	float ReloadTime = 1;

	/** Played for the reload (UpperBody slot keeps the legs moving). Empty = the hero's DeadlockAnimSet->ReloadMontage. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;

protected:
	float GetReloadDuration() const;
	UAnimMontage* FindReloadMontage() const;
};
