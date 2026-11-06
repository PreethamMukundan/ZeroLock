// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_DeadlyPlumage.h"

#include "GAS/BaseCharAttributeSet.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "GAS/ZL_GameplayTags.h"

UZL_GE_Xayah_Plumage::UZL_GE_Xayah_Plumage()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

	FSetByCallerFloat Multiplier;
	Multiplier.DataTag = ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_FIRE_RATE_MULTIPLIER;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UBaseCharAttributeSet::GetFireRateAttribute();
	Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Multiplier);
	Modifiers.Add(Modifier);
}

UZL_Xayah_DeadlyPlumage::UZL_Xayah_DeadlyPlumage()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	Slot = EGameplayAbilitySlot::AbilitySlot2;
	CooldownDuration = 18.0f;
	AbilityName = TEXT("Deadly Plumage");
	AbilityDescription = TEXT("Your weapon fires 50% faster for 5 seconds.");
}

void UZL_Xayah_DeadlyPlumage::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float Level = GetAbilityLevel();
	const float Duration = BuffDuration.GetValueAtLevel(Level);
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UZL_GE_Xayah_Plumage::StaticClass(), Level);
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(Duration, true);
		Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_FIRE_RATE_MULTIPLIER,
			1.0f / (1.0f + FMath::Max(FireRateBonus.GetValueAtLevel(Level), 0.0f)));
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}

	BuffTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, Duration);
	BuffTask->OnProgress.AddDynamic(this, &UZL_Xayah_DeadlyPlumage::UpdateProgressionTimer);
	BuffTask->OnStarted.AddDynamic(this, &UZL_Xayah_DeadlyPlumage::StartProgressionTimer);
	BuffTask->OnEnd.AddDynamic(this, &UZL_Xayah_DeadlyPlumage::StopProgressionTimer);
	BuffTask->OnFinished.AddDynamic(this, &UZL_Xayah_DeadlyPlumage::OnBuffFinished);
	BuffTask->ReadyForActivation();
}

void UZL_Xayah_DeadlyPlumage::OnBuffFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
