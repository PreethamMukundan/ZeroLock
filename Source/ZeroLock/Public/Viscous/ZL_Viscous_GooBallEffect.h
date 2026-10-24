// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZL_Viscous_GooBallEffect.generated.h"

/**
 * On Viscous while he's a Goo Ball. Infinite (UZL_Viscous_GooBall removes it): adds WeaponResistance and
 * SpiritResistance (percent) from SetByCaller TAG_SETBYCALLER_BULLET_RESIST / TAG_SETBYCALLER_SPIRIT_RESIST.
 */
UCLASS()
class ZEROLOCK_API UZL_Viscous_GooBallEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_Viscous_GooBallEffect();
};
