// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_HotelGuestEffect.h"

UZL_Doorman_HotelGuestEffect::UZL_Doorman_HotelGuestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
}
