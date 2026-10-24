// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_GooBallEffect.h"

#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"

UZL_Viscous_GooBallEffect::UZL_Viscous_GooBallEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	auto AddResist = [this](const FGameplayAttribute& Attribute, const FGameplayTag& Tag)
	{
		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = Tag;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		Modifiers.Add(Modifier);
	};
	AddResist(UBaseCharAttributeSet::GetWeaponResistanceAttribute(), ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_BULLET_RESIST);
	AddResist(UBaseCharAttributeSet::GetSpiritResistanceAttribute(), ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_SPIRIT_RESIST);
}
