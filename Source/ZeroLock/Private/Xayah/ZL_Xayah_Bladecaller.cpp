// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_Bladecaller.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Solomon/ZL_Solomon_KnightEffect.h"
#include "Xayah/ZL_Xayah_Feather.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Xayah_Bladecaller::UZL_Xayah_Bladecaller()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	Slot = EGameplayAbilitySlot::AbilitySlot3;
	CooldownDuration = 12.0f;
	AbilityName = TEXT("Bladecaller");
	AbilityDescription = TEXT("Call your feathers back. Each one damages enemies it passes; 3 hits on one enemy roots them.");
}

bool UZL_Xayah_Bladecaller::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ActorInfo && AZL_Xayah_Feather::GetGroundFeathers(ActorInfo->AvatarActor.Get()).Num() > 0;
}

void UZL_Xayah_Bladecaller::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		const float Level = GetAbilityLevel();
		CastDamage = SpiritDamage.GetValueAtLevel(Level);
		CastRootHitCount = FMath::Max(FMath::RoundToInt(RootHitCount.GetValueAtLevel(Level)), 1);
		CastRootDuration = RootDuration.GetValueAtLevel(Level);
		FeatherHits.Reset();

		for (AZL_Xayah_Feather* Feather : AZL_Xayah_Feather::GetGroundFeathers(GetAvatarActorFromActorInfo()))
		{
			Feather->StartReturn(this);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UZL_Xayah_Bladecaller::OnFeatherHit(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan)
{
	UBaseCharAbilitySystemComponent* HeroASC = IsValid(Hero) ? Hero->GetMyAbilitySystemComp() : nullptr;
	UBaseCharAbilitySystemComponent* VillanASC = IsValid(Villan) ? Villan->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC || !VillanASC)
	{
		return;
	}

	HeroASC->ApplySpiritDamage(VillanASC, CastDamage, this);

	int32& Hits = FeatherHits.FindOrAdd(Villan);
	if (++Hits == CastRootHitCount)
	{
		// Root = Mover "Locked" while the tag is on.
		FGameplayTagContainer RootTags;
		RootTags.AddTag(ZerolockGameplayTagsForBinding::TAG_MOVEMENT_ROOTED);
		UZL_Solomon_KnightEffect::Apply(HeroASC, VillanASC, CastRootDuration, RootTags);
	}
}
