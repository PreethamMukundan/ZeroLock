// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZL_Pocket_AfflictionEffect.generated.h"

/**
 * Affliction's damage over time. Every period it runs UCalc_Spirit_Damage, the same way GE_BaseSpiritDamage does,
 * with the per-tick damage passed as SetByCaller Zerolock.DamageCalc.Spirit.
 * UZL_Pocket_Affliction sets the duration, period and granted tags on the spec, so this class needs no tuning.
 * It doesn't stack: recasting removes the old one first (a stack can't change its granted tags when the level changes).
 */
UCLASS()
class ZEROLOCK_API UZL_Pocket_AfflictionEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_Pocket_AfflictionEffect();
};
