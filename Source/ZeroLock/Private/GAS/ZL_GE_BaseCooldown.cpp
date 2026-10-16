//Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GE_BaseCooldown.h"

#include "GAS/ZL_GameplayTags.h"


UZL_GE_BaseCooldown::UZL_GE_BaseCooldown()
{
	DurationPolicy=EGameplayEffectDurationType::HasDuration;

	// Length comes from UBaseGameplayAbility::ApplyCooldown's SetByCaller. Without this the duration is 0, which GAS treats as instant.
	FSetByCallerFloat CooldownDuration;
	CooldownDuration.DataTag = ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_COOLDOWN_DURATION;
	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownDuration);
}
