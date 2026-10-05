//Copyright Preetham Mukundan (C) 2026


#include "GAS/Calculations/Calc_WeaponDamage.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"


struct Zero_WeaponDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponResistance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponResistanceReduction);
	DECLARE_ATTRIBUTE_CAPTUREDEF(FlatWeapon);
	DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponLifeSteal);
	DECLARE_ATTRIBUTE_CAPTUREDEF(UniversalDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);

	Zero_WeaponDamageStatics()
	{

		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,WeaponDamage, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,Damage, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,FlatWeapon, Source,true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,WeaponLifeSteal,Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,UniversalDamage, Source, true);
		
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,WeaponResistanceReduction,Target,false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet, WeaponResistance, Target, false);
	}
};
static const Zero_WeaponDamageStatics& WeaponDamageStatics()
{
	static Zero_WeaponDamageStatics ZWStatics;
	return ZWStatics;
}
UCalc_WeaponDamage::UCalc_WeaponDamage()
{
	RelevantAttributesToCapture.Add(WeaponDamageStatics().WeaponDamageDef);
	RelevantAttributesToCapture.Add(WeaponDamageStatics().FlatWeaponDef);
	RelevantAttributesToCapture.Add(WeaponDamageStatics().WeaponLifeStealDef);
	RelevantAttributesToCapture.Add(WeaponDamageStatics().WeaponResistanceDef);
	RelevantAttributesToCapture.Add(WeaponDamageStatics().WeaponResistanceReductionDef);
	RelevantAttributesToCapture.Add(WeaponDamageStatics().DamageDef);
	RelevantAttributesToCapture.Add(WeaponDamageStatics().UniversalDamageDef);
}

void UCalc_WeaponDamage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
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

	float WeaponResistance = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(WeaponDamageStatics().WeaponResistanceDef, EvaluationParameters, WeaponResistance);
	WeaponResistance = FMath::Max<float>(WeaponResistance, 0.0f);

	float WeaponResistanceReduction = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(WeaponDamageStatics().WeaponResistanceReductionDef, EvaluationParameters, WeaponResistanceReduction);
	WeaponResistanceReduction = FMath::Max<float>(WeaponResistanceReduction, 0.0f);

	float WeaponDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(WeaponDamageStatics().WeaponDamageDef, EvaluationParameters, WeaponDamage);
	WeaponDamage = FMath::Max<float>(WeaponDamage, 0.0f);

	float FlatWeapon = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(WeaponDamageStatics().FlatWeaponDef, EvaluationParameters, FlatWeapon);
	FlatWeapon = FMath::Max<float>(FlatWeapon, 0.0f);

	float WeaponLifeSteal = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(WeaponDamageStatics().WeaponLifeStealDef, EvaluationParameters, WeaponLifeSteal);
	WeaponLifeSteal = FMath::Max<float>(WeaponLifeSteal, 0.0f);

	float Damage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(WeaponDamageStatics().DamageDef, EvaluationParameters, Damage);

	float DamageBase = Damage + FlatWeapon;
	
	float UnmitigatedDamage = DamageBase + (DamageBase * WeaponDamage/100);

	float UniversalDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(WeaponDamageStatics().UniversalDamageDef, EvaluationParameters, UniversalDamage);
	UniversalDamage = FMath::Max<float>(UniversalDamage, 0.0f);
	UnmitigatedDamage *= 1.0f + (UniversalDamage/100);

	float NetWeaponResistance = WeaponResistance - WeaponResistanceReduction;
	
	float MitigatedDamage = (UnmitigatedDamage) * (1- (NetWeaponResistance/100));

	FGameplayEffectSpec* MutableSpec = ExecutionParams.GetOwningSpecForPreExecuteMod();
	MutableSpec->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(FName("Damage.Tag.Weapon")));
	

	if (MitigatedDamage >= 0.f)
	{
		if (UBaseCharAbilitySystemComponent* mySourceASC = Cast<UBaseCharAbilitySystemComponent>(SourceAbilitySystemComponent))
		{
			float HealAmout = MitigatedDamage * (WeaponLifeSteal/100);
			mySourceASC->ApplyHeal(mySourceASC,HealAmout);
		}
		// Set the Target's damage meta attribute
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(WeaponDamageStatics().DamageProperty, EGameplayModOp::Additive, MitigatedDamage));
	}
}
