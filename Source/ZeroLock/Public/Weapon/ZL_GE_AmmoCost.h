// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZL_GE_AmmoCost.generated.h"

/**
 * Primary fire ammo cost. Instant: adds SetByCaller TAG_SETBYCALLER_AMMO to CurrentAmmo (UZL_GA_WeaponFire::ApplyCost
 * sets it to minus the shots fired). Applied inside the fire ability's prediction window, so the owning client's ammo
 * count drops at once.
 */
UCLASS()
class ZEROLOCK_API UZL_GE_AmmoCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_AmmoCost();
};
