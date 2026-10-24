// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GE_MoveSlow.h"

#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"

UZL_GE_MoveSlow::UZL_GE_MoveSlow()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FSetByCallerFloat Multiplier;
	Multiplier.DataTag = ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_MOVE_SPEED_MULTIPLIER;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UBaseCharAttributeSet::GetMoveSpeedMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Multiplier);
	Modifiers.Add(Modifier);
}
