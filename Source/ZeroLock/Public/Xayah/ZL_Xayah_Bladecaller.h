// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Xayah_Bladecaller.generated.h"

class AZeroLockCharacter;

/**
 * Slot 3. Calls every feather on the ground back to Xayah. Each feather deals spirit damage to the enemies it passes, and an
 * enemy hit by RootHitCount feathers in one cast is rooted. Can't be used without a feather on the ground.
 */
UCLASS()
class ZEROLOCK_API UZL_Xayah_Bladecaller : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Xayah_Bladecaller();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Server, from a returning feather (they outlive the ability's activation, so the values are cached at the cast). */
	void OnFeatherHit(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan);

protected:
	/** Per feather. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat SpiritDamage = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat RootHitCount = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat RootDuration = 1.25f;

private:
	float CastDamage = 0.0f;
	int32 CastRootHitCount = 0;
	float CastRootDuration = 0.0f;
	TMap<TWeakObjectPtr<AZeroLockCharacter>, int32> FeatherHits;
};
