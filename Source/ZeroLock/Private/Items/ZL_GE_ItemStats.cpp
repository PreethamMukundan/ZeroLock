// Copyright Preetham Mukundan (C) 2026

#include "Items/ZL_GE_ItemStats.h"

#include "AbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"

namespace ZLItemStats
{
	struct FStatDefinition
	{
		EZL_ItemStat Stat;
		FGameplayAttribute Attribute;
		TEnumAsByte<EGameplayModOp::Type> Op;
	};

	static const TArray<FStatDefinition>& GetDefinitions()
	{
		using Attributes = UBaseCharAttributeSet;
		static const TArray<FStatDefinition> Definitions = {
			{ EZL_ItemStat::MaxHealth, Attributes::GetMaximumHealthAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::WeaponDamage, Attributes::GetWeaponDamageAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::MeleeDamage, Attributes::GetMeleeDamageAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::SpiritPower, Attributes::GetSpiritDamageAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::WeaponResist, Attributes::GetWeaponResistanceAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::SpiritResist, Attributes::GetSpiritResistanceAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::MeleeResist, Attributes::GetMeleeResistanceAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::WeaponLifesteal, Attributes::GetWeaponLifeStealAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::SpiritLifesteal, Attributes::GetSpiritLifeStealAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::CooldownReduction, Attributes::GetCooldownReductionAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::HealingBonus, Attributes::GetHealingBonusAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::MaxStamina, Attributes::GetMaxStaminaAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AmmoFlat, Attributes::GetMaxAmmoAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AmmoPercent, Attributes::GetMaxAmmoAttribute(), EGameplayModOp::MultiplyAdditive },
			{ EZL_ItemStat::StaminaRegenPercent, Attributes::GetStaminaRegenRateAttribute(), EGameplayModOp::MultiplyAdditive },
			// FireRate is seconds between shots, so a faster gun divides it.
			{ EZL_ItemStat::FireRatePercent, Attributes::GetFireRateAttribute(), EGameplayModOp::DivideAdditive },
			{ EZL_ItemStat::HealthRegen, Attributes::GetHealthRegenerationAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::OutOfCombatRegen, Attributes::GetOutOfCombatRegenAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::MoveSpeed, Attributes::GetMoveSpeedBonusAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::SprintSpeed, Attributes::GetSprintSpeedBonusAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::BulletSpeedPercent, Attributes::GetBulletSpeedBonusAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::DebuffResist, Attributes::GetDebufReductionAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::SlowResist, Attributes::GetSlowResistanceAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::SlideDistance, Attributes::GetSlideDistanceBonusAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::MeleeDistance, Attributes::GetMeleeDistanceBonusAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AirControl, Attributes::GetAirControlBonusAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AbilityDuration, Attributes::GetDurationExtensionAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AbilityRange, Attributes::GetAbilityRangeBonusAttribute(), EGameplayModOp::AddBase },
			// Every charged ability, whichever slot it uses. Slots without one ignore their MaxCharges.
			{ EZL_ItemStat::AbilityCharges, Attributes::GetMaxCharges_1Attribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AbilityCharges, Attributes::GetMaxCharges_2Attribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AbilityCharges, Attributes::GetMaxCharges_3Attribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::AbilityCharges, Attributes::GetMaxCharges_4Attribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::ChargeCooldownReduction, Attributes::GetChargeCooldownReductionAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::SpiritPowerPercent, Attributes::GetSpiritDamageAttribute(), EGameplayModOp::MultiplyAdditive },
			// Negative values: -24 multiplies by 0.76. A UZL_GE_MoveSlow override still wins over it.
			{ EZL_ItemStat::MoveSlowPercent, Attributes::GetMoveSpeedMultiplierAttribute(), EGameplayModOp::MultiplyAdditive },
			{ EZL_ItemStat::AllDamagePercent, Attributes::GetUniversalDamageAttribute(), EGameplayModOp::AddBase },
			{ EZL_ItemStat::HealingReduction, Attributes::GetHealingReductionAttribute(), EGameplayModOp::AddBase },
		};
		return Definitions;
	}

	static FName GetDataName(EZL_ItemStat Stat)
	{
		return FName(TEXT("ItemStat"), static_cast<int32>(Stat) + 1);
	}

	static bool IsPercentOfBase(const FStatDefinition& Definition)
	{
		return Definition.Op != EGameplayModOp::AddBase;
	}
}

FGameplayTag ZLItemStatus::GetTag(EZL_ItemStatus Status)
{
	using namespace ZerolockGameplayTagsForBinding;
	switch (Status)
	{
	case EZL_ItemStatus::Silence: return TAG_STATUS_SILENCED;
	case EZL_ItemStatus::Disarm: return TAG_STATUS_DISARMED;
	case EZL_ItemStatus::Stun: return TAG_STUN;
	case EZL_ItemStatus::BulletImmune: return TAG_STATUS_BULLET_IMMUNE;
	case EZL_ItemStatus::Unstoppable: return TAG_STATUS_UNSTOPPABLE;
	case EZL_ItemStatus::Stealth: return TAG_STATUS_STEALTHED;
	case EZL_ItemStatus::Invulnerable: return TAG_UNTOUCHABLE;
	case EZL_ItemStatus::Enlarged: return TAG_STATUS_ENLARGED;
	case EZL_ItemStatus::Shrunk: return TAG_STATUS_SHRUNK;
	default: return FGameplayTag();
	}
}

const FGameplayTagContainer& ZLItemStatus::GetCrowdControlTags()
{
	using namespace ZerolockGameplayTagsForBinding;
	static const FGameplayTagContainer Tags = []
	{
		FGameplayTagContainer Container;
		Container.AddTag(TAG_STUN);
		Container.AddTag(TAG_STATUS_SILENCED);
		Container.AddTag(TAG_STATUS_DISARMED);
		Container.AddTag(TAG_MOVEMENT_ROOTED);
		return Container;
	}();
	return Tags;
}

void ZLItemStatus::Apply(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, EZL_ItemStatus Status, float Duration)
{
	const FGameplayTag Tag = GetTag(Status);
	if (!Source || !Target || !Tag.IsValid() || Duration <= 0.0f)
	{
		return;
	}

	if (Status == EZL_ItemStatus::Unstoppable)
	{
		Target->RemoveActiveEffectsWithGrantedTags(GetCrowdControlTags());
	}

	const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(UZL_GE_ItemStatus::StaticClass(), 1.0f, Source->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(Duration, true);
		Spec.Data->DynamicGrantedTags.AddTag(Tag);
		Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
	}
}

UZL_GE_ItemStatus::UZL_GE_ItemStatus()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	// Placeholder: the applier always calls SetDuration.
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
}

UZL_GE_ItemStats::UZL_GE_ItemStats()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	for (const ZLItemStats::FStatDefinition& Definition : ZLItemStats::GetDefinitions())
	{
		FSetByCallerFloat Magnitude;
		Magnitude.DataName = ZLItemStats::GetDataName(Definition.Stat);

		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Definition.Attribute;
		Modifier.ModifierOp = Definition.Op;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	}
}

UZL_GE_ItemStatsTimed::UZL_GE_ItemStatsTimed()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	// Placeholder: the applier always calls SetDuration.
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
}

void UZL_GE_ItemStats::SetMagnitudes(FGameplayEffectSpec& Spec, const TArray<FZL_ItemStatValue>& Stats)
{
	for (const ZLItemStats::FStatDefinition& Definition : ZLItemStats::GetDefinitions())
	{
		float Total = 0.f;
		for (const FZL_ItemStatValue& Value : Stats)
		{
			if (Value.Stat == Definition.Stat)
			{
				Total += Value.Value;
			}
		}

		// Multiply and divide modifiers count from 1, so +20% is 1.2 and no bonus is 1.
		const float Magnitude = ZLItemStats::IsPercentOfBase(Definition) ? 1.f + Total / 100.f : Total;
		Spec.SetSetByCallerMagnitude(ZLItemStats::GetDataName(Definition.Stat), Magnitude);
	}
}
