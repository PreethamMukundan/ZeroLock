// Copyright Preetham Mukundan (C) 2026

#include "Items/ZL_GA_ActiveItem.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/BaseCharAttributeSet.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Items/ZL_ItemProcComponent.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace
{
	/** Everyone the item hits: the target, or the right team within the Radius of the cast point. */
	TArray<AZeroLockCharacter*> FindHeroesHit(const FZL_ActiveItem& Active, AZeroLockCharacter* Hero, AZeroLockCharacter* Target, const FVector& Location)
	{
		TArray<AZeroLockCharacter*> Heroes;
		if (Active.Radius <= 0.0f)
		{
			if (Target)
			{
				Heroes.Add(Target);
			}
			return Heroes;
		}

		TArray<FOverlapResult> Overlaps;
		Hero->GetWorld()->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
			FCollisionShape::MakeSphere(Active.Radius));

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AZeroLockCharacter* Candidate = Cast<AZeroLockCharacter>(Overlap.GetActor());
			if (!Candidate || Candidate->bIsDead)
			{
				continue;
			}
			const bool bAlly = Candidate == Hero || Hero->IsOnSameTeam(Candidate);
			if (bAlly == (Active.AreaTeam == EZL_ActiveItemTeam::Allies))
			{
				Heroes.AddUnique(Candidate);
			}
		}
		return Heroes;
	}

	constexpr float DashTime = 0.15f;
	constexpr float PullTime = 0.25f;
	constexpr float WallGap = 60.0f;

	void ApplyTimedStats(UBaseGameplayAbility* Ability, const TArray<FZL_ItemStatValue>& Stats, float Duration, UBaseCharAbilitySystemComponent* HeroASC, UAbilitySystemComponent* TargetASC)
	{
		FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
		Context.SetAbility(Ability);

		const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(UZL_GE_ItemStatsTimed::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetDuration(Duration, true);
			UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, Stats);
			HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}

	/** Where a dash ends: DashDistance ahead, or short of the first wall. */
	FVector FindDashEnd(const AZeroLockCharacter* Hero, float Distance)
	{
		const FVector Start = Hero->GetActorLocation();
		const FVector Forward = Hero->GetActorForwardVector().GetSafeNormal2D();
		FVector End = Start + Forward * Distance;

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLItemDash), false, Hero);
		if (Hero->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			End = Hit.Location - Forward * WallGap;
		}
		return End;
	}

	void ExecuteNow(UBaseGameplayAbility* Ability, const UZero_Item_data* Item, AZeroLockCharacter* Hero, AZeroLockCharacter* Target, const FVector& Location)
	{
		UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
		const FZL_ActiveItem& Active = Item->Active;

		for (AZeroLockCharacter* Struck : FindHeroesHit(Active, Hero, Target, Location))
		{
			UBaseCharAbilitySystemComponent* StruckASC = Struck->GetMyAbilitySystemComp();
			if (!StruckASC)
			{
				continue;
			}

			const bool bHostile = Struck != Hero && !Hero->IsOnSameTeam(Struck);
			if (bHostile && HeroASC->IsUntouchable(StruckASC))
			{
				continue;
			}

			if (Active.Damage > 0.0f)
			{
				HeroASC->ApplySpiritDamage(StruckASC, Active.Damage, Ability);
			}
			const float Heal = Active.Heal + Struck->GetMyAttributeSet()->GetMaximumHealth() * Active.HealMaxHealthPercent / 100.0f;
			if (Heal > 0.0f)
			{
				HeroASC->ApplyHeal(StruckASC, Heal);
			}
			if (Active.Barrier > 0.0f)
			{
				StruckASC->AddCombatBarrier(Active.Barrier, Active.BarrierDuration);
			}
			if (!Active.TimedStats.IsEmpty() && Active.Duration > 0.0f)
			{
				ApplyTimedStats(Ability, Active.TimedStats, Active.Duration, HeroASC, StruckASC);
			}
			if (Active.bCleanse)
			{
				StruckASC->CleanseDebuffs();
			}
			for (const EZL_ItemStatus Status : Active.Statuses)
			{
				ZLItemStatus::Apply(HeroASC, StruckASC, Status, Active.StatusDuration);
			}
			if ((Active.DotDamagePerSecond > 0.0f || Active.DotMaxHealthPercent > 0.0f) && Hero->GetItemProcs())
			{
				Hero->GetItemProcs()->AddDot(Struck, Active.DotDamagePerSecond, Active.DotMaxHealthPercent, Active.DotDuration);
			}
			if (Active.bPullToPoint)
			{
				UBaseGameplayAbility::SendVictimMoveEvent(Hero, Struck, TEXT("Zerolock.Movement.PullTo"), PullTime, &Location);
			}
		}

		if (!Active.CasterStats.IsEmpty() && Active.Duration > 0.0f)
		{
			ApplyTimedStats(Ability, Active.CasterStats, Active.Duration, HeroASC, HeroASC);
		}
		for (const EZL_ItemStatus Status : Active.CasterStatuses)
		{
			ZLItemStatus::Apply(HeroASC, HeroASC, Status, Active.StatusDuration);
		}
		if (Active.ZoneDuration > 0.0f && Hero->GetItemProcs())
		{
			Hero->GetItemProcs()->AddZone(Location, Active.Radius, Active.ZoneDuration, Active.ZoneDamagePerSecond);
		}

		if (Active.LaunchSpeed > 0.0f)
		{
			const FVector Launch(0.0f, 0.0f, Active.LaunchSpeed);
			UBaseGameplayAbility::SendVictimMoveEvent(Hero, Hero, TEXT("Zerolock.Movement.Forced"), 0.1f, &Launch);
		}
		if (Active.DashDistance > 0.0f)
		{
			const FVector DashEnd = FindDashEnd(Hero, Active.DashDistance);
			UBaseGameplayAbility::SendVictimMoveEvent(Hero, Hero, TEXT("Zerolock.Movement.PullTo"), DashTime, &DashEnd);
		}
		if (Active.bDashToTarget && Target)
		{
			const FVector TargetLocation = Target->GetActorLocation();
			UBaseGameplayAbility::SendVictimMoveEvent(Hero, Hero, TEXT("Zerolock.Movement.PullTo"), DashTime, &TargetLocation);
		}
	}
}

TSubclassOf<UBaseGameplayAbility> ZLActiveItem::GetAbilityClass(EZL_ActiveItemTarget Target)
{
	switch (Target)
	{
	case EZL_ActiveItemTarget::Self: return UZL_GA_ActiveItem_Self::StaticClass();
	case EZL_ActiveItemTarget::Ally:
	case EZL_ActiveItemTarget::Enemy: return UZL_GA_ActiveItem_Target::StaticClass();
	case EZL_ActiveItemTarget::Ground: return UZL_GA_ActiveItem_Ground::StaticClass();
	default: return nullptr;
	}
}

FGameplayTag ZLActiveItem::GetCooldownTag(int32 Slot)
{
	using namespace ZerolockGameplayTagsForBinding;
	switch (Slot)
	{
	case 0: return TAG_COOLDOWN_ITEM_SLOT_1;
	case 1: return TAG_COOLDOWN_ITEM_SLOT_2;
	case 2: return TAG_COOLDOWN_ITEM_SLOT_3;
	case 3: return TAG_COOLDOWN_ITEM_SLOT_4;
	default: return FGameplayTag();
	}
}

const UZero_Item_data* ZLActiveItem::SetUpAbility(UBaseGameplayAbility& Ability, const FGameplayAbilitySpec& Spec)
{
	const UZero_Item_data* Item = Cast<UZero_Item_data>(Spec.SourceObject.Get());
	if (Item)
	{
		Ability.CooldownDuration.SetValue(Item->Active.Cooldown);
		Ability.CooldownTags = Spec.GetDynamicSpecSourceTags();
	}
	return Item;
}

void ZLActiveItem::Execute(UBaseGameplayAbility* Ability, const UZero_Item_data* Item, AZeroLockCharacter* Hero, AZeroLockCharacter* Target, const FVector& Location)
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!Item || !HeroASC || !Hero->HasAuthority())
	{
		return;
	}

	if (Item->Active.Delay <= 0.0f)
	{
		ExecuteNow(Ability, Item, Hero, Target, Location);
		return;
	}

	TWeakObjectPtr<UBaseGameplayAbility> WeakAbility(Ability);
	TWeakObjectPtr<AZeroLockCharacter> WeakTarget(Target);
	FTimerHandle Handle;
	Hero->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Hero, [WeakAbility, Item, Hero, WeakTarget, Location]()
	{
		if (!Hero->bIsDead)
		{
			ExecuteNow(WeakAbility.Get(), Item, Hero, WeakTarget.Get(), Location);
		}
	}), Item->Active.Delay, false);
}

UZL_GA_ActiveItem_Self::UZL_GA_ActiveItem_Self()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UZL_GA_ActiveItem_Self::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	Item = ZLActiveItem::SetUpAbility(*this, Spec);
}

void UZL_GA_ActiveItem_Self::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero)
	{
		ZLActiveItem::Execute(this, Item, Hero, Hero, Hero->GetActorLocation());
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UZL_GA_ActiveItem_Target::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	Item = ZLActiveItem::SetUpAbility(*this, Spec);
	if (Item)
	{
		const bool bAlly = Item->Active.Target == EZL_ActiveItemTarget::Ally;
		Range.SetValue(Item->Active.Range);
		TargetTeam = bAlly ? EZL_ConeTargetTeam::Allies : EZL_ConeTargetTeam::Enemies;
		bSelfCastOnDoubleTap = bAlly;
		bSelfCastWithoutTarget = bAlly;
	}
}

void UZL_GA_ActiveItem_Target::ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data)
{
	if (Target)
	{
		ZLActiveItem::Execute(this, Item, Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()), Target, Target->GetActorLocation());
	}
	Super::ConeTargetConfirmed(Target, Data);
}

UZL_GA_ActiveItem_Ground::UZL_GA_ActiveItem_Ground()
{
	SurfaceMode = EZL_SurfaceTargetMode::Ground;
}

void UZL_GA_ActiveItem_Ground::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	Item = ZLActiveItem::SetUpAbility(*this, Spec);
	if (Item)
	{
		MaxPlacementRange.SetValue(Item->Active.Range);
	}
}

void UZL_GA_ActiveItem_Ground::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	ZLActiveItem::Execute(this, Item, Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()), nullptr, Hit.ImpactPoint);
	Super::PlacementConfirmed(Hit, Placement, Data);
}
