//Copyright Preetham Mukundan (C) 2026


#include "GAS/Calculations/Calc_Spirit_Damage.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"


struct Zero_SpiritDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(SpiritDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(FlatSpirit);
	DECLARE_ATTRIBUTE_CAPTUREDEF(SpiritLifeSteal);
	DECLARE_ATTRIBUTE_CAPTUREDEF(UniversalDamage);

	DECLARE_ATTRIBUTE_CAPTUREDEF(SpiritResistance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(SpiritResistanceReduction);

	Zero_SpiritDamageStatics()
	{

		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,SpiritDamage, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,Damage, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,FlatSpirit, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,SpiritLifeSteal, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,UniversalDamage, Source, true);
		
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet, SpiritResistance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet, SpiritResistanceReduction, Target, false);
	}
};
static const Zero_SpiritDamageStatics& SpiritDamageStatics()
{
	static Zero_SpiritDamageStatics ZSStatics;
	return ZSStatics;
}
UCalc_Spirit_Damage::UCalc_Spirit_Damage()
{
	RelevantAttributesToCapture.Add(SpiritDamageStatics().SpiritDamageDef);
	RelevantAttributesToCapture.Add(SpiritDamageStatics().FlatSpiritDef);
	RelevantAttributesToCapture.Add(SpiritDamageStatics().SpiritLifeStealDef);
	RelevantAttributesToCapture.Add(SpiritDamageStatics().SpiritResistanceDef);
	RelevantAttributesToCapture.Add(SpiritDamageStatics().SpiritResistanceReductionDef);
	RelevantAttributesToCapture.Add(SpiritDamageStatics().DamageDef);
	RelevantAttributesToCapture.Add(SpiritDamageStatics().UniversalDamageDef);
}

void UCalc_Spirit_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
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

	float SpiritResistance = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(SpiritDamageStatics().SpiritResistanceDef, EvaluationParameters, SpiritResistance);
	SpiritResistance = FMath::Max<float>(SpiritResistance, 0.0f);

	float SpiritResistanceReduction = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(SpiritDamageStatics().SpiritResistanceReductionDef, EvaluationParameters, SpiritResistanceReduction);
	SpiritResistanceReduction = FMath::Max<float>(SpiritResistanceReduction, 0.0f);

	float SpiritDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(SpiritDamageStatics().SpiritDamageDef, EvaluationParameters, SpiritDamage);
	SpiritDamage = FMath::Max<float>(SpiritDamage, 0.0f);

	float FlatSpirit = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(SpiritDamageStatics().FlatSpiritDef, EvaluationParameters, FlatSpirit);
	FlatSpirit = FMath::Max<float>(FlatSpirit, 0.0f);

	float SpiritLifeSteal = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(SpiritDamageStatics().SpiritLifeStealDef, EvaluationParameters, SpiritLifeSteal);
	SpiritLifeSteal = FMath::Max<float>(SpiritLifeSteal, 0.0f);

	float Damage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(SpiritDamageStatics().DamageDef, EvaluationParameters, Damage);

	float BaseSpiritDamage = Damage+FlatSpirit;
	
	float UnmitigatedDamage = BaseSpiritDamage + (BaseSpiritDamage * SpiritDamage/100); // Can multiply any damage boosters here

	float UniversalDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(SpiritDamageStatics().UniversalDamageDef, EvaluationParameters, UniversalDamage);
	UniversalDamage = FMath::Max<float>(UniversalDamage, 0.0f);
	UnmitigatedDamage *= 1.0f + (UniversalDamage/100);


	float NetSpiritResistance = SpiritResistance - SpiritResistanceReduction;
	float MitigatedDamage = (UnmitigatedDamage) * (1- (NetSpiritResistance/100));

	FGameplayEffectSpec* MutableSpec = ExecutionParams.GetOwningSpecForPreExecuteMod();
	MutableSpec->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(FName("Damage.Tag.Spirit")));
	
	
	if (MitigatedDamage >= 0.f)
	{

		if (UBaseCharAbilitySystemComponent* mySourceASC = Cast<UBaseCharAbilitySystemComponent>(SourceAbilitySystemComponent))
		{
			float HealAmout = MitigatedDamage * (SpiritLifeSteal/100);
			mySourceASC->ApplyHeal(mySourceASC,HealAmout);
		}
		// Set the Target's damage meta attribute
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(SpiritDamageStatics().DamageProperty, EGameplayModOp::Additive, MitigatedDamage));
	}
}
