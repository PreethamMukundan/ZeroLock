// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GAS/BasePassive_GameplayAbility.h"
#include "ZL_Xayah_CleanCut.generated.h"

/** Clean Cut shots ready to fire, one stack each. Duration set per spec; grants TAG_STATUS_CLEAN_CUT through the spec. */
UCLASS()
class ZEROLOCK_API UZL_GE_Xayah_CleanCut : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_Xayah_CleanCut();
};

/**
 * Xayah's passive. Using an ability readies ShotsPerAbility Clean Cut shots (up to the effect's stack limit): her next
 * weapon bullets drop a feather where they land (AZL_Xayah_WeaponProjectile). Server only.
 */
UCLASS()
class ZEROLOCK_API UZL_Xayah_CleanCut : public UBasePassive_GameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Xayah_CleanCut();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Server: spends one Clean Cut shot. False if she has none. */
	static bool ConsumeFeatherShot(UAbilitySystemComponent* ASC);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Clean Cut")
	FScalableFloat ShotsPerAbility = 3.0f;

	/** Seconds the shots stay ready; each ability refreshes it. */
	UPROPERTY(EditDefaultsOnly, Category = "Clean Cut")
	FScalableFloat ShotDuration = 8.0f;

private:
	void OnAbilityCommitted(UGameplayAbility* Ability);

	FDelegateHandle AbilityCommittedHandle;
};
