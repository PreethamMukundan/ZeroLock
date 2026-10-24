// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZL_Viscous_CubeEffect.generated.h"

/**
 * On a hero encased in Viscous's Cube. Infinite: AZL_Viscous_Cube applies it with Zerolock.Untouchable and
 * Zerolock.Status.Cubed on the spec and removes it when the cube ends.
 */
UCLASS()
class ZEROLOCK_API UZL_Viscous_CubeEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_Viscous_CubeEffect();
};
