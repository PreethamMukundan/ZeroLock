// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Xayah/ZL_Xayah_ThrowAbility.h"
#include "ZL_Xayah_Double_daggers.generated.h"

/** Slot 1. Throws two blades that fly through enemies and drop a feather where they stop. */
UCLASS()
class ZEROLOCK_API UZL_Xayah_Double_daggers : public UZL_Xayah_ThrowAbility
{
	GENERATED_BODY()

public:
	UZL_Xayah_Double_daggers();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
