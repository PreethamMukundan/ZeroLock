// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_AfflictionEffect.h"

#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/Calculations/Calc_Spirit_Damage.h"

UZL_Pocket_AfflictionEffect::UZL_Pocket_AfflictionEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
	Period = 1.0f;
	bExecutePeriodicEffectOnApplication = true;

	FSetByCallerFloat SpiritDamage;
	SpiritDamage.DataTag = ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_SPIRIT;

	FGameplayEffectExecutionScopedModifierInfo DamageModifier(FGameplayEffectAttributeCaptureDefinition(UBaseCharAttributeSet::GetDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, true));
	DamageModifier.ModifierOp = EGameplayModOp::AddBase;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SpiritDamage);

	FGameplayEffectExecutionDefinition SpiritExecution;
	SpiritExecution.CalculationClass = UCalc_Spirit_Damage::StaticClass();
	SpiritExecution.CalculationModifiers.Add(DamageModifier);
	Executions.Add(SpiritExecution);
}
