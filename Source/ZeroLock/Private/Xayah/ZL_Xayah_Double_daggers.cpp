// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_Double_daggers.h"

UZL_Xayah_Double_daggers::UZL_Xayah_Double_daggers()
{
	Slot = EGameplayAbilitySlot::AbilitySlot1;
	CooldownDuration = 9.0f;
	SpiritDamage = 55.0f;
	BladeCount = 2;
	FanAngle = 5.0f;
	AbilityName = TEXT("Double Daggers");
	AbilityDescription = TEXT("Throw two blades that pierce enemies and leave a feather where they stop.");
}

void UZL_Xayah_Double_daggers::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ThrowFan();
}
