// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Items/Zero_Item_data.h"
#include "ZL_GE_ItemStats.generated.h"

class UAbilitySystemComponent;

namespace ZLItemStatus
{
	FGameplayTag GetTag(EZL_ItemStatus Status);

	/** Everything Unstoppable blocks. */
	const FGameplayTagContainer& GetCrowdControlTags();

	/** Server. */
	void Apply(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, EZL_ItemStatus Status, float Duration);
}

/**
 * The one effect behind every item's Stats list: an infinite effect with a SetByCaller modifier per EZL_ItemStat,
 * so items need no effect asset of their own. Applied by UZero_Item_Inventory_Component.
 */
UCLASS()
class ZEROLOCK_API UZL_GE_ItemStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_ItemStats();

	/** Sets every modifier's magnitude, including the stats the item doesn't have: an unset SetByCaller is an error. */
	static void SetMagnitudes(FGameplayEffectSpec& Spec, const TArray<FZL_ItemStatValue>& Stats);
};

/** The same stats for a limited time: what an active item applies. The applier sets the duration on the spec. */
UCLASS()
class ZEROLOCK_API UZL_GE_ItemStatsTimed : public UZL_GE_ItemStats
{
	GENERATED_BODY()

public:
	UZL_GE_ItemStatsTimed();
};

/** Carries one status tag for a limited time. The applier adds the tag and calls SetDuration: see ZLItemStatus::Apply. */
UCLASS()
class ZEROLOCK_API UZL_GE_ItemStatus : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_ItemStatus();
};
