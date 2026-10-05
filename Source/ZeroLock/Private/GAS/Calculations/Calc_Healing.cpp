//Copyright Preetham Mukundan (C) 2026


#include "GAS/Calculations/Calc_Healing.h"

#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"

struct Zero_HealStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(HealingBonus);
	DECLARE_ATTRIBUTE_CAPTUREDEF(HealingReduction);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Healing);

	

	Zero_HealStatics()
	{

		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,HealingBonus, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,Healing, Source, true);

		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,HealingReduction, Target, false);
	}
};
static const Zero_HealStatics& HealingStatics()
{
	static Zero_HealStatics ZHStatics;
	return ZHStatics;
}
UCalc_Healing::UCalc_Healing()
{
	RelevantAttributesToCapture.Add(Zero_HealStatics().HealingBonusDef);
	RelevantAttributesToCapture.Add(Zero_HealStatics().HealingReductionDef);
	RelevantAttributesToCapture.Add(Zero_HealStatics().HealingDef);
}

void UCalc_Healing::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* TargetAbilitySystemComponent = ExecutionParams.GetTargetAbilitySystemComponent();
	UAbilitySystemComponent* SourceAbilitySystemComponent = ExecutionParams.GetSourceAbilitySystemComponent();

	AActor* SourceActor = SourceAbilitySystemComponent ? SourceAbilitySystemComponent->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetAbilitySystemComponent ? TargetAbilitySystemComponent->GetAvatarActor() : nullptr;

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// Gather the tags from the source and target as that can affect which buffs should be used
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	// Level 3 Affliction (and anything else that grants it) stops all healing.
	if (TargetTags && TargetTags->HasTag(ZerolockGameplayTagsForBinding::TAG_STATUS_HEAL_BLOCKED))
	{
		return;
	}


	float HealingBonus = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HealingStatics().HealingBonusDef, EvaluationParameters, HealingBonus);
	HealingBonus = FMath::Max<float>(HealingBonus, 0.0f);

	float HealingReduction = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HealingStatics().HealingReductionDef, EvaluationParameters, HealingReduction);
	HealingReduction = FMath::Max<float>(HealingReduction, 0.0f);

	float Healing = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HealingStatics().HealingDef, EvaluationParameters, Healing);

	
	
	float HealingIncreace = Healing *(1+((HealingBonus/100)-(HealingReduction/100)));


	FGameplayEffectSpec* MutableSpec = ExecutionParams.GetOwningSpecForPreExecuteMod();
	MutableSpec->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(FName("Damage.Tag.Heal")));
	

	if (HealingIncreace >= 0.f)
	{
		// Set the Target's damage meta attribute
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(HealingStatics().HealingProperty, EGameplayModOp::Additive, HealingIncreace));
	}
}
