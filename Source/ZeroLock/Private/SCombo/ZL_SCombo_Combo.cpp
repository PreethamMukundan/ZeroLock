// Copyright Preetham Mukundan (C) 2026


#include "SCombo/ZL_SCombo_Combo.h"

#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Items/Zero_Item_data.h"
#include "SCombo/ZL_SCombo_ComboBar.h"
#include "SCombo/ZL_SCombo_Projectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

int32 ZLSCombo::GetComboStacks(UAbilitySystemComponent* ASC)
{
	return ASC ? ASC->GetGameplayEffectCount(UZL_GE_SCombo_Combo::StaticClass(), nullptr) : 0;
}

int32 ZLSCombo::GetMaxComboStacks()
{
	return GetDefault<UZL_GE_SCombo_Combo>()->GetStackLimitCount();
}

void ZLSCombo::FindEnemiesInRadius(AZeroLockCharacter* Hero, float Radius, TArray<AZeroLockCharacter*>& OutVillans)
{
	OutVillans.Reset();
	UWorld* World = Hero ? Hero->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FVector Center = Hero->GetActorLocation();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_SCombo_Area), false, Hero);
	TArray<FOverlapResult> Overlaps;
	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || OutVillans.Contains(Villan))
		{
			continue;
		}
		FCollisionQueryParams WallParams(SCENE_QUERY_STAT(ZL_SCombo_AreaWall), false, Hero);
		WallParams.AddIgnoredActor(Villan);
		if (!World->LineTraceTestByObjectType(Center, Villan->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), WallParams))
		{
			OutVillans.Add(Villan);
		}
	}
}

UZL_GE_SCombo_Combo::UZL_GE_SCombo_Combo()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
	// The setters are editor-only, and a native effect needs these in game builds too.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 10;
}

UZL_SCombo_ComboPassive::UZL_SCombo_ComboPassive()
{
	AbilityName = TEXT("Combo");
	AbilityDescription = TEXT("Landing damage from a different source than your last hit adds a combo stack. Stacks fade over time.");
	AbilityHUDWidget = UZL_SCombo_ComboBar::StaticClass();
}

void UZL_SCombo_ComboPassive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Intentionally not calling Super: it waits for weapon hits, and this passive watches all damage instead.
	if (UBaseCharAbilitySystemComponent* ASC = Cast<UBaseCharAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
	{
		DamageDealtHandle = ASC->OnDamageDealt.AddUObject(this, &UZL_SCombo_ComboPassive::OnDamageDealt);
	}
}

void UZL_SCombo_ComboPassive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (DamageDealtHandle.IsValid())
	{
		if (UBaseCharAbilitySystemComponent* ASC = ActorInfo ? Cast<UBaseCharAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr)
		{
			ASC->OnDamageDealt.Remove(DamageDealtHandle);
		}
		DamageDealtHandle.Reset();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

EGASAbilityInputID UZL_SCombo_ComboPassive::GetComboSource(EZL_ProcTrigger HitTrigger, const UObject* DamageSource)
{
	// Fired by the slot's slash, so it's the same source as the slash.
	if (Cast<AZL_SCombo_Projectile>(DamageSource))
	{
		return EGASAbilityInputID::Secondry_Attack;
	}

	if (const UGameplayAbility* Ability = Cast<UGameplayAbility>(DamageSource))
	{
		const FGameplayAbilitySpec* Spec = Ability->GetCurrentAbilitySpec();
		const EGASAbilityInputID Input = Spec ? static_cast<EGASAbilityInputID>(Spec->InputID) : EGASAbilityInputID::None;
		switch (Input)
		{
		case EGASAbilityInputID::Secondry_Attack:
		case EGASAbilityInputID::Ability_1:
		case EGASAbilityInputID::Ability_2:
			return Input;
		case EGASAbilityInputID::Ultimate:
			return EGASAbilityInputID::None;
		default:
			break;
		}
	}

	// By trigger only now: the slash deals weapon damage too, but was caught above.
	switch (HitTrigger)
	{
	case EZL_ProcTrigger::BulletHit:
		return EGASAbilityInputID::Primary_Attack;
	case EZL_ProcTrigger::MeleeHit:
		return EGASAbilityInputID::Melee;
	default:
		return EGASAbilityInputID::None;
	}
}

void UZL_SCombo_ComboPassive::OnDamageDealt(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger HitTrigger, const UObject* DamageSource)
{
	const EGASAbilityInputID Source = GetComboSource(HitTrigger, DamageSource);
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (Source == EGASAbilityInputID::None || !ASC)
	{
		return;
	}

	// An empty combo starts fresh, whatever landed last.
	if (ZLSCombo::GetComboStacks(ASC) > 0 && Source == LastComboSource)
	{
		return;
	}
	LastComboSource = Source;

	const float Level = GetAbilityLevel();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UZL_GE_SCombo_Combo::StaticClass(), Level, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(StackDecayTime.GetValueAtLevel(Level), true);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}
