// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZL_Doorman_HotelGuestEffect.generated.h"

/**
 * On a guest of Doorman's hotel for the stay. AZL_Doorman_Hotel::CheckIn sets the duration (the time limit) and grants
 * Zerolock.Status.HotelGuest + Zerolock.Status.MovementAbilitiesBlocked on the spec; it removes the effect at checkout.
 */
UCLASS()
class ZEROLOCK_API UZL_Doorman_HotelGuestEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_Doorman_HotelGuestEffect();
};
