// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "GAS/ZL_BaseConeTargetAbility.h"
#include "GAS/ZL_BaseSurfaceTargetAbility.h"
#include "Items/Zero_Item_data.h"
#include "ZL_GA_ActiveItem.generated.h"

class AZeroLockCharacter;

/**
 * What the three active item abilities share. Each owned active item gets its own spec, with the item as the source object
 * and the slot's key as the input ID, so the ability classes hold no item data of their own.
 */
namespace ZLActiveItem
{
	/** The ability class that casts this target type, null for None. */
	TSubclassOf<UBaseGameplayAbility> GetAbilityClass(EZL_ActiveItemTarget Target);

	/** The cooldown tag of an active slot (0-3). */
	FGameplayTag GetCooldownTag(int32 Slot);

	/** Reads the spec's item and sets the instance's cooldown. Null if the spec has no item. */
	const UZero_Item_data* SetUpAbility(UBaseGameplayAbility& Ability, const FGameplayAbilitySpec& Spec);

	/** Server: applies the item's effects to Target, or around Location when the item has a Radius. */
	void Execute(UBaseGameplayAbility* Ability, const UZero_Item_data* Item, AZeroLockCharacter* Hero, AZeroLockCharacter* Target, const FVector& Location);
}

/** Press to cast on yourself, or on everyone in the item's Radius around you. */
UCLASS()
class ZEROLOCK_API UZL_GA_ActiveItem_Self : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_GA_ActiveItem_Self();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UZero_Item_data> Item;
};

/** Aim at a hero with the cone, then confirm. Ally items can also self-cast. */
UCLASS()
class ZEROLOCK_API UZL_GA_ActiveItem_Target : public UZL_BaseConeTargetAbility
{
	GENERATED_BODY()

public:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	virtual void ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UZero_Item_data> Item;
};

/** Pick a spot on the ground, then confirm. */
UCLASS()
class ZEROLOCK_API UZL_GA_ActiveItem_Ground : public UZL_BaseSurfaceTargetAbility
{
	GENERATED_BODY()

public:
	UZL_GA_ActiveItem_Ground();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UZero_Item_data> Item;
};
