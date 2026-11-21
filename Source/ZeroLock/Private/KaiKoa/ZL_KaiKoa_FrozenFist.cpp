// Copyright Preetham Mukundan (C) 2026


#include "KaiKoa/ZL_KaiKoa_FrozenFist.h"

#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_StatusTimerAbility.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Items/Zero_Item_data.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_GE_KaiKoa_Frost::UZL_GE_KaiKoa_Frost()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.0f));
	// The setters are editor-only, and a native effect needs these in game builds too.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 20;
}

int32 UZL_GE_KaiKoa_Frost::GetStacks(const UAbilitySystemComponent* TargetASC)
{
	return TargetASC ? TargetASC->GetGameplayEffectCount(UZL_GE_KaiKoa_Frost::StaticClass(), nullptr) : 0;
}

UZL_KaiKoa_FrozenFist::UZL_KaiKoa_FrozenFist()
{
	FrostTriggers = { EZL_ProcTrigger::MeleeHit, EZL_ProcTrigger::HeavyMeleeHit };
	AbilityName = TEXT("Frozen Fist");
	AbilityDescription = TEXT("Melee hits add frost to the enemy; all of it fades 3 s after the last hit. At 3 stacks the enemy freezes for 1.5 s and the frost clears.");
}

void UZL_KaiKoa_FrozenFist::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Intentionally not calling Super: it waits for weapon hits, and this passive watches Kai's melee damage instead.
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (UBaseCharAbilitySystemComponent* ASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr)
	{
		DamageDealtHandle = ASC->OnDamageDealt.AddUObject(this, &UZL_KaiKoa_FrozenFist::OnDamageDealt);
	}
}

void UZL_KaiKoa_FrozenFist::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (DamageDealtHandle.IsValid())
	{
		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
		if (UBaseCharAbilitySystemComponent* ASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr)
		{
			ASC->OnDamageDealt.Remove(DamageDealtHandle);
		}
		DamageDealtHandle.Reset();
	}
	LastStackTime.Reset();
	FrozenUntil.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZL_KaiKoa_FrozenFist::OnDamageDealt(AZeroLockCharacter* Villan, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !Hero->HasAuthority() || !IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan)
		|| !FrostTriggers.Contains(Trigger))
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Until = FrozenUntil.Find(Villan); Until && Now < *Until)
	{
		return;
	}
	if (const double* Last = LastStackTime.Find(Villan); Last && Now - *Last < StackCooldownPerTarget)
	{
		return;
	}
	LastStackTime.Add(Villan, Now);

	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC)
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(UZL_GE_KaiKoa_Frost::StaticClass(), 1.0f, HeroASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetDuration(FMath::Max(FrostDecayTime, 0.1f), true);
	Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_DEBUFFSTACK_FROST);
	for (int32 Index = 0; Index < FMath::Max(StacksPerHit, 1); ++Index)
	{
		HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, VillanASC);
	}

	const FRotator Facing = (Villan->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D().Rotation();
	if (UZL_GE_KaiKoa_Frost::GetStacks(VillanASC) < FMath::Max(StacksToFreeze, 1))
	{
		ZLKitCue::Play(VillanASC, FrostCue, Villan->GetActorLocation(), Facing);
		return;
	}

	// Freeze: the frost clears and the enemy is stunned and rooted.
	VillanASC->RemoveActiveGameplayEffectBySourceEffect(UZL_GE_KaiKoa_Frost::StaticClass(), nullptr);
	const float Duration = FreezeDuration.GetValueAtLevel(GetAbilityLevel());
	FrozenUntil.Add(Villan, Now + Duration);
	if (Duration > 0.0f && !HeroASC->IsUntouchable(VillanASC))
	{
		for (const EZL_ItemStatus Status : { EZL_ItemStatus::Stun, EZL_ItemStatus::Root })
		{
			ZLItemStatus::Apply(HeroASC, VillanASC, Status, Duration);
			UZL_StatusTimerAbility::SendStatusTimer(Hero, VillanASC, ZLItemStatus::GetTag(Status), Duration);
		}
		ZLKitCue::Play(VillanASC, FrozenCue, Villan->GetActorLocation(), Facing);
	}
}
