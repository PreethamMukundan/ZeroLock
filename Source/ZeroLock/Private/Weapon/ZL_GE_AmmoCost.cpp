// Copyright Preetham Mukundan (C) 2026


#include "Weapon/ZL_GE_AmmoCost.h"

#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"

UZL_GE_AmmoCost::UZL_GE_AmmoCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Ammo;
	Ammo.DataTag = ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_AMMO;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UBaseCharAttributeSet::GetCurrentAmmoAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Ammo);
	Modifiers.Add(Modifier);
}
