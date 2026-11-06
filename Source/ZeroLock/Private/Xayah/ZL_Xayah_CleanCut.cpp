// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_CleanCut.h"

#include "AbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "ZeroLock/ZeroLock.h"

UZL_GE_Xayah_CleanCut::UZL_GE_Xayah_CleanCut()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
	// The setter is editor-only, and a native effect needs this in game builds too.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 5;
}

UZL_Xayah_CleanCut::UZL_Xayah_CleanCut()
{
	AbilityName = TEXT("Clean Cut");
	AbilityDescription = TEXT("Using an ability readies your next 3 shots: each drops a feather where it lands.");
}

void UZL_Xayah_CleanCut::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Intentionally not calling Super: it waits for weapon hits, and this passive watches abilities instead.
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		AbilityCommittedHandle = ASC->AbilityCommittedCallbacks.AddUObject(this, &UZL_Xayah_CleanCut::OnAbilityCommitted);
	}
}

void UZL_Xayah_CleanCut::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AbilityCommittedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->AbilityCommittedCallbacks.Remove(AbilityCommittedHandle);
		}
		AbilityCommittedHandle.Reset();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZL_Xayah_CleanCut::OnAbilityCommitted(UGameplayAbility* Ability)
{
	const FGameplayAbilitySpec* CommittedSpec = Ability ? Ability->GetCurrentAbilitySpec() : nullptr;
	if (!CommittedSpec)
	{
		return;
	}

	// Her four active abilities; not the weapon, reload, melee or item actives.
	switch (static_cast<EGASAbilityInputID>(CommittedSpec->InputID))
	{
	case EGASAbilityInputID::Secondry_Attack:
	case EGASAbilityInputID::Ability_1:
	case EGASAbilityInputID::Ability_2:
	case EGASAbilityInputID::Ultimate:
		break;
	default:
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	const float Level = GetAbilityLevel();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UZL_GE_Xayah_CleanCut::StaticClass(), Level, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(ShotDuration.GetValueAtLevel(Level), true);
		Spec.Data->SetStackCount(FMath::Max(FMath::RoundToInt(ShotsPerAbility.GetValueAtLevel(Level)), 1));
		Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_CLEAN_CUT);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

bool UZL_Xayah_CleanCut::ConsumeFeatherShot(UAbilitySystemComponent* ASC)
{
	if (!ASC || ASC->GetGameplayEffectCount(UZL_GE_Xayah_CleanCut::StaticClass(), nullptr) <= 0)
	{
		return false;
	}
	ASC->RemoveActiveGameplayEffectBySourceEffect(UZL_GE_Xayah_CleanCut::StaticClass(), nullptr, 1);
	return true;
}
