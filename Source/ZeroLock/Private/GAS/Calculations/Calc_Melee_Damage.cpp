//Copyright Preetham Mukundan (C) 2026


#include "GAS/Calculations/Calc_Melee_Damage.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"

struct Zero_MeleeDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(MeleeDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(MeleeLifeSteal);
	DECLARE_ATTRIBUTE_CAPTUREDEF(UniversalDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponDamage);

	DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponResistance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponResistanceReduction);
	DECLARE_ATTRIBUTE_CAPTUREDEF(MeleeResistance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(MeleeResistanceReduction);
	

	Zero_MeleeDamageStatics()
	{

		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,WeaponDamage, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,Damage, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,MeleeDamage, Source,true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,MeleeLifeSteal,Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,UniversalDamage, Source, true);
		
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet,WeaponResistanceReduction,Target,false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet, WeaponResistance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet, MeleeResistance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseCharAttributeSet, MeleeResistanceReduction, Target, false);
	}
};
static const Zero_MeleeDamageStatics& MeleeDamageStatics()
{
	static Zero_MeleeDamageStatics ZMStatics;
	return ZMStatics;
}
UCalc_Melee_Damage::UCalc_Melee_Damage()
{
	RelevantAttributesToCapture.Add(MeleeDamageStatics().WeaponDamageDef);
	RelevantAttributesToCapture.Add(MeleeDamageStatics().MeleeDamageDef);
	RelevantAttributesToCapture.Add(MeleeDamageStatics().MeleeLifeStealDef);
	RelevantAttributesToCapture.Add(MeleeDamageStatics().DamageDef);
	RelevantAttributesToCapture.Add(MeleeDamageStatics().UniversalDamageDef);
	
	RelevantAttributesToCapture.Add(MeleeDamageStatics().WeaponResistanceDef);
	RelevantAttributesToCapture.Add(MeleeDamageStatics().MeleeResistanceDef);
	RelevantAttributesToCapture.Add(MeleeDamageStatics().WeaponResistanceReductionDef);
	RelevantAttributesToCapture.Add(MeleeDamageStatics().MeleeResistanceReductionDef);
}

void UCalc_Melee_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
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
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().WeaponResistanceDef, EvaluationParameters, WeaponResistance);
	WeaponResistance = FMath::Max<float>(WeaponResistance, 0.0f);

	float WeaponResistanceReduction = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().WeaponResistanceReductionDef, EvaluationParameters, WeaponResistanceReduction);
	WeaponResistanceReduction = FMath::Max<float>(WeaponResistanceReduction, 0.0f);

	float WeaponDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().WeaponDamageDef, EvaluationParameters, WeaponDamage);
	WeaponDamage = FMath::Max<float>(WeaponDamage, 0.0f);

	float MeleeDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().MeleeDamageDef, EvaluationParameters, MeleeDamage);
	MeleeDamage = FMath::Max<float>(MeleeDamage, 0.0f);

	float MeleeResistance = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().MeleeResistanceDef, EvaluationParameters, MeleeResistance);
	MeleeResistance = FMath::Max<float>(MeleeResistance, 0.0f);

	float MeleeResistanceReduction = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().MeleeResistanceReductionDef, EvaluationParameters, MeleeResistanceReduction);
	MeleeResistanceReduction = FMath::Max<float>(MeleeResistanceReduction, 0.0f);

	float MeleeLifeSteal = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().MeleeLifeStealDef, EvaluationParameters, MeleeLifeSteal);
	MeleeLifeSteal = FMath::Max<float>(MeleeLifeSteal, 0.0f);

	float Damage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().DamageDef, EvaluationParameters, Damage);

	float DamageBase = Damage;
	
	float UnmitigatedDamage = DamageBase + ((DamageBase * WeaponDamage/100)*0.5) + (DamageBase * MeleeDamage/100);

	float UniversalDamage = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MeleeDamageStatics().UniversalDamageDef, EvaluationParameters, UniversalDamage);
	UniversalDamage = FMath::Max<float>(UniversalDamage, 0.0f);
	UnmitigatedDamage *= 1.0f + (UniversalDamage/100);

	float NetWeaponResistance = WeaponResistance - WeaponResistanceReduction;
	float NetMeleeResistance = MeleeResistance - MeleeResistanceReduction;
	
	float MitigatedDamage = (UnmitigatedDamage) * (1- (NetWeaponResistance/100) * (1-NetMeleeResistance/100));

	FGameplayEffectSpec* MutableSpec = ExecutionParams.GetOwningSpecForPreExecuteMod();
	MutableSpec->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(FName("Damage.Tag.Melee")));
	

	if (MitigatedDamage >= 0.f)
	{
		if (UBaseCharAbilitySystemComponent* mySourceASC = Cast<UBaseCharAbilitySystemComponent>(SourceAbilitySystemComponent))
		{
			float HealAmout = MitigatedDamage * (MeleeLifeSteal/100);
			mySourceASC->ApplyHeal(mySourceASC,HealAmout);
		}
		// Set the Target's damage meta attribute
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(MeleeDamageStatics().DamageProperty, EGameplayModOp::Additive, MitigatedDamage));
	}
}
