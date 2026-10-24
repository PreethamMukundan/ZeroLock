// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZL_GE_MoveSlow.generated.h"

/**
 * Movement slow. Infinite: whoever applies it removes it (e.g. leaving a Viscous goo puddle). Overrides MoveSpeedMultiplier
 * with SetByCaller TAG_SETBYCALLER_MOVE_SPEED_MULTIPLIER (0.6 = 40% slower), so overlapping slows don't stack: the most
 * recent active one wins. Add TAG_STATUS_SLOWED to the spec's DynamicGrantedTags when applying.
 */
UCLASS()
class ZEROLOCK_API UZL_GE_MoveSlow : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_MoveSlow();
};
