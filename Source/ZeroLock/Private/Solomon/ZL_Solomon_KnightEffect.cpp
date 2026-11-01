// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_KnightEffect.h"

#include "AbilitySystemComponent.h"

UZL_Solomon_KnightEffect::UZL_Solomon_KnightEffect()
{
	// The real duration is set per spec (Apply).
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
}

FActiveGameplayEffectHandle UZL_Solomon_KnightEffect::Apply(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, float Duration,
	const FGameplayTagContainer& GrantedTags)
{
	if (!Source || !Target || Duration <= 0.0f || !Source->IsOwnerActorAuthoritative())
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.AddSourceObject(Source->GetAvatarActor());
	const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	Spec.Data->SetDuration(Duration, true);
	Spec.Data->DynamicGrantedTags.AppendTags(GrantedTags);
	return Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target);
}
