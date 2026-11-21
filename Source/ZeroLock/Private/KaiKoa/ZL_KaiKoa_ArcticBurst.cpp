// Copyright Preetham Mukundan (C) 2026


#include "KaiKoa/ZL_KaiKoa_ArcticBurst.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Items/ZL_GE_ItemStats.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_KaiKoa_ArcticBurst::UZL_KaiKoa_ArcticBurst()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	AbilityInputID = EGASAbilityInputID::Ability_2;
	Slot = EGameplayAbilitySlot::AbilitySlot3;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_KAIKOA_ARCTICBURST);
	CooldownDuration = 14.0f;
	AbilityName = TEXT("Arctic Burst");
	AbilityDescription = TEXT("Burst ice in front of you: 60 spirit damage and a 40% slow for 1.5 s to every enemy caught in it.");
}

void UZL_KaiKoa_ArcticBurst::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Forward = GetHeroAimDirection().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = Hero->GetActorForwardVector().GetSafeNormal2D();
	}
	const float Radius = BurstRadius.GetValueAtLevel(GetAbilityLevel()) * GetAbilityRangeScale();
	const FVector Center = Hero->GetActorLocation() + Forward * BurstForwardOffset * GetAbilityRangeScale();

	// Inside the activation: predicted on the owner, the server's multicast skips them.
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), BurstCue, Center, Forward.Rotation(), Radius);

	if (HasAuthority(&ActivationInfo))
	{
		TArray<AZeroLockCharacter*> Villans;
		FindTargets(Hero, Center, Radius, Villans);
		for (AZeroLockCharacter* Villan : Villans)
		{
			Chill(Hero, Villan, Center);
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			DrawDebugSphere(GetWorld(), Center, Radius, 24, Villans.Num() > 0 ? FColor::Green : FColor::Red, false, 2.0f);
		}
#endif
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UZL_KaiKoa_ArcticBurst::FindTargets(AZeroLockCharacter* Hero, const FVector& Center, float Radius, TArray<AZeroLockCharacter*>& OutVillans) const
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_KaiKoa_ArcticBurst), false, Hero);

	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || OutVillans.Contains(Villan))
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (!VillanASC || HeroASC->IsUntouchable(VillanASC))
		{
			continue;
		}

		// Walls between the centre and the enemy block it.
		FHitResult Hit;
		FCollisionQueryParams LOSParams(SCENE_QUERY_STAT(ZL_KaiKoa_ArcticBurstLOS), false, Hero);
		LOSParams.AddIgnoredActor(Villan);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Center, Villan->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), LOSParams))
		{
			continue;
		}
		OutVillans.Add(Villan);
	}
}

void UZL_KaiKoa_ArcticBurst::Chill(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan, const FVector& Center) const
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC)
	{
		return;
	}

	const int32 Level = GetAbilityLevel();
	HeroASC->ApplySpiritDamage(VillanASC, SpiritDamage.GetValueAtLevel(Level), this);
	if (Villan->bIsDead)
	{
		return;
	}

	const float Slow = SlowPercent.GetValueAtLevel(Level);
	const float Duration = SlowDuration.GetValueAtLevel(Level);
	if (Slow > 0.0f && Duration > 0.0f)
	{
		FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
		Context.SetAbility(this);
		const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(UZL_GE_ItemStatsTimed::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			FZL_ItemStatValue SlowStat;
			SlowStat.Stat = EZL_ItemStat::MoveSlowPercent;
			SlowStat.Value = -Slow;
			Spec.Data->SetDuration(Duration, true);
			UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, { SlowStat });
			HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, VillanASC);
		}
	}

	ZLKitCue::Play(VillanASC, ChillCue, Villan->GetActorLocation(), (Villan->GetActorLocation() - Center).GetSafeNormal2D().Rotation());
}
