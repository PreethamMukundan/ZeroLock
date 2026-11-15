// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_KitTypes.h"

#include "Abilities/GameplayAbility.h"
#include "Components/CapsuleComponent.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/BaseGameplayAbility.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_StatusTimerAbility.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Items/ZL_ItemProcComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

bool FZL_KitPayload::IsEmpty() const
{
	return Damage <= 0.f && ExecuteBelowHealthPercent <= 0.f && DotPerSecond <= 0.f && Heal <= 0.f && HealMaxHealthPercent <= 0.f
		&& Barrier <= 0.f && !bStripBarrier && !bStealBarrier && !bCleanse && Stats.IsEmpty() && Statuses.IsEmpty() && TimedTags.IsEmpty()
		&& FMath::IsNearlyZero(KnockbackSpeed) && FMath::IsNearlyZero(KnockUpSpeed) && !bPullToOrigin && FMath::IsNearlyZero(CooldownDelay);
}

bool ZLKit::MatchesTeam(AZeroLockCharacter* Hero, AZeroLockCharacter* Other, EZL_KitTeam Team)
{
	if (!IsValid(Hero) || !IsValid(Other) || Other->bIsDead)
	{
		return false;
	}
	const bool bSelf = Other == Hero;
	const bool bAlly = bSelf || Hero->IsOnSameTeam(Other);
	switch (Team)
	{
	case EZL_KitTeam::Enemies: return !bAlly;
	case EZL_KitTeam::Allies: return bAlly;
	case EZL_KitTeam::AlliesNotSelf: return bAlly && !bSelf;
	case EZL_KitTeam::Everyone: return !bSelf;
	default: return false;
	}
}

void ZLKit::FindHeroes(AZeroLockCharacter* Hero, const FVector& Origin, const FVector& Direction, const FZL_KitArea& Area, float RangeScale,
	EZL_KitTeam Team, TArray<AZeroLockCharacter*>& OutHeroes)
{
	if (!IsValid(Hero))
	{
		return;
	}
	UWorld* World = Hero->GetWorld();
	const float Range = Area.Range * RangeScale;
	const FVector Flat = Direction.GetSafeNormal2D().IsNearlyZero() ? Hero->GetActorForwardVector().GetSafeNormal2D() : Direction.GetSafeNormal2D();

	// Broad phase: a sphere that holds the whole shape.
	float QueryRadius = Range;
	if (Area.Shape == EZL_KitShape::Line)
	{
		QueryRadius = FMath::Sqrt(FMath::Square(Range) + FMath::Square(Area.Angle));
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(QueryRadius + 50.f));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Candidate = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!MatchesTeam(Hero, Candidate, Team) || OutHeroes.Contains(Candidate))
		{
			continue;
		}

		const FVector ToTarget = Candidate->GetActorLocation() - Origin;
		const FVector ToTargetFlat(ToTarget.X, ToTarget.Y, 0.f);
		const float CapsuleRadius = Candidate->GetCapsuleComponent() ? Candidate->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
		bool bInside = false;
		switch (Area.Shape)
		{
		case EZL_KitShape::Sphere:
			bInside = ToTarget.Size() <= Range + CapsuleRadius;
			break;
		case EZL_KitShape::Cone:
		{
			const float Distance = ToTargetFlat.Size();
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ToTargetFlat.GetSafeNormal(), Flat)));
			bInside = Distance <= Range + CapsuleRadius && (Distance < CapsuleRadius * 2.f || Angle <= Area.Angle);
			break;
		}
		case EZL_KitShape::Line:
		{
			const float Along = FVector::DotProduct(ToTargetFlat, Flat);
			const float Side = FMath::Abs(FVector::CrossProduct(Flat, ToTargetFlat).Z);
			bInside = Along >= -CapsuleRadius && Along <= Range + CapsuleRadius && Side <= Area.Angle + CapsuleRadius;
			break;
		}
		}
		if (!bInside)
		{
			continue;
		}

		if (Area.bRequireLineOfSight)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLKitLOS), false, Hero);
			Params.AddIgnoredActor(Candidate);
			if (World->LineTraceSingleByObjectType(Hit, Origin, Candidate->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				continue;
			}
		}
		OutHeroes.Add(Candidate);
	}
}

void ZLKit::ApplyTimedTags(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, const FGameplayTagContainer& Tags, float Duration)
{
	if (!Source || !Target || Tags.IsEmpty() || Duration <= 0.f)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(UZL_GE_ItemStatus::StaticClass(), 1.0f, Source->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(Duration, true);
		Spec.Data->DynamicGrantedTags.AppendTags(Tags);
		Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
	}
}

void ZLKit::ApplyPayload(const UObject* Source, AZeroLockCharacter* Hero, AZeroLockCharacter* Target, const FVector& Origin,
	const FZL_KitPayload& Payload, float Scale)
{
	if (!IsValid(Hero) || !IsValid(Target) || !Hero->HasAuthority() || Target->bIsDead)
	{
		return;
	}
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* TargetASC = Target->GetMyAbilitySystemComp();
	if (!HeroASC || !TargetASC)
	{
		return;
	}

	const bool bHostile = Target != Hero && !Hero->IsOnSameTeam(Target);
	if (bHostile && HeroASC->IsUntouchable(TargetASC))
	{
		return;
	}
	using namespace ZerolockGameplayTagsForBinding;

	// Barrier first, so a steal or strip takes it before the damage lands.
	if (Payload.bStealBarrier || Payload.bStripBarrier)
	{
		const float TargetBarrier = TargetASC->GetNumericAttribute(UBaseCharAttributeSet::GetCombatBarrierAttribute());
		if (TargetBarrier > 0.f)
		{
			TargetASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetCombatBarrierAttribute(), 0.f);
			if (Payload.bStealBarrier && Target != Hero)
			{
				HeroASC->AddCombatBarrier(TargetBarrier, Payload.StolenBarrierDuration);
			}
		}
	}

	if (bHostile && Payload.Damage > 0.f)
	{
		const float Damage = Payload.Damage * Scale;
		switch (Payload.DamageType)
		{
		case EZL_KitDamageType::Weapon: HeroASC->ApplyWeaponDamage(TargetASC, Damage, Source); break;
		case EZL_KitDamageType::Melee: HeroASC->ApplyMeleeDamage(TargetASC, Damage, Source); break;
		default: HeroASC->ApplySpiritDamage(TargetASC, Damage, Source); break;
		}
	}

	if (bHostile && Payload.ExecuteBelowHealthPercent > 0.f && !Target->bIsDead)
	{
		const UBaseCharAttributeSet* Attributes = Target->GetMyAttributeSet();
		const float MaxHealth = Attributes ? Attributes->GetMaximumHealth() : 0.f;
		if (MaxHealth > 0.f && Attributes->GetCurrentHealth() / MaxHealth * 100.f <= Payload.ExecuteBelowHealthPercent)
		{
			// Bigger than any health pool after resists and barrier.
			HeroASC->ApplySpiritDamage(TargetASC, 1000000.f, Source);
		}
	}
	if (Target->bIsDead)
	{
		return;
	}

	if (bHostile && Payload.DotPerSecond > 0.f && Payload.DotDuration > 0.f && Hero->GetItemProcs())
	{
		Hero->GetItemProcs()->AddDot(Target, Payload.DotPerSecond * Scale, 0.f, Payload.DotDuration);
	}

	const float MaxHealth = Target->GetMyAttributeSet() ? Target->GetMyAttributeSet()->GetMaximumHealth() : 0.f;
	const float Heal = Payload.Heal * Scale + MaxHealth * Payload.HealMaxHealthPercent / 100.f;
	if (!bHostile && Heal > 0.f)
	{
		HeroASC->ApplyHeal(TargetASC, Heal);
	}
	if (!bHostile && Payload.Barrier > 0.f)
	{
		TargetASC->AddCombatBarrier(Payload.Barrier * Scale, Payload.BarrierDuration > 0.f ? Payload.BarrierDuration : 5.f);
	}
	if (!bHostile && Payload.bCleanse)
	{
		TargetASC->CleanseDebuffs();
	}

	if (!Payload.Stats.IsEmpty() && Payload.StatDuration > 0.f)
	{
		FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
		if (const UGameplayAbility* Ability = Cast<UGameplayAbility>(Source))
		{
			Context.SetAbility(Ability);
		}
		const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(UZL_GE_ItemStatsTimed::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetDuration(Payload.StatDuration, true);
			UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, Payload.Stats);
			HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}

	for (const EZL_ItemStatus Status : Payload.Statuses)
	{
		ZLItemStatus::Apply(HeroASC, TargetASC, Status, Payload.StatusDuration);
		if (bHostile && (Status == EZL_ItemStatus::Stun || Status == EZL_ItemStatus::Root))
		{
			UZL_StatusTimerAbility::SendStatusTimer(Hero, TargetASC, ZLItemStatus::GetTag(Status), Payload.StatusDuration);
		}
	}
	ApplyTimedTags(HeroASC, TargetASC, Payload.TimedTags, Payload.TagDuration);

	if (!FMath::IsNearlyZero(Payload.CooldownDelay))
	{
		// Positive reduces there, so a delay is a negative reduction.
		TargetASC->AdjustActiveEffectsDurationByValue(-Payload.CooldownDelay);
	}

	if (!FMath::IsNearlyZero(Payload.KnockbackSpeed) || !FMath::IsNearlyZero(Payload.KnockUpSpeed))
	{
		FVector Away = (Target->GetActorLocation() - Origin).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = Target == Hero ? Hero->GetActorForwardVector().GetSafeNormal2D() : (Target->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D();
		}
		const FVector Velocity = Away * Payload.KnockbackSpeed + FVector::UpVector * Payload.KnockUpSpeed;
		UBaseGameplayAbility::SendVictimMoveEvent(Hero, Target, TEXT("Zerolock.Movement.Forced"), FMath::Max(Payload.KnockbackDuration, 0.05f), &Velocity);
	}
	if (Payload.bPullToOrigin)
	{
		UBaseGameplayAbility::SendVictimMoveEvent(Hero, Target, TEXT("Zerolock.Movement.PullTo"), FMath::Max(Payload.PullDuration, 0.05f), &Origin);
	}

	if (Payload.HitCue.IsSet())
	{
		const FRotator Facing = (Target->GetActorLocation() - Origin).GetSafeNormal2D().Rotation();
		ZLKitCue::Play(TargetASC, Payload.HitCue, Target->GetActorLocation(), Facing);
	}
}

void ZLKit::DrawArea(const UWorld* World, const FVector& Origin, const FVector& Direction, const FZL_KitArea& Area, float RangeScale, const FColor& Color)
{
#if ENABLE_DRAW_DEBUG
	const float Range = Area.Range * RangeScale;
	const FVector Flat = Direction.GetSafeNormal2D();
	switch (Area.Shape)
	{
	case EZL_KitShape::Sphere:
		ZLDebugDraw::Sphere(World, Origin, Range, Color, 1.5f, 24);
		break;
	case EZL_KitShape::Cone:
		ZLDebugDraw::Cone(World, Origin, Flat, Range, FMath::DegreesToRadians(Area.Angle), FMath::DegreesToRadians(15.f), Color, 1.5f);
		break;
	case EZL_KitShape::Line:
	{
		// A row of spheres the line's width along its length.
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Range / FMath::Max(Area.Angle, 25.f)), 1, 12);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			ZLDebugDraw::Sphere(World, Origin + Flat * (Range * Step / Steps), Area.Angle, Color, 1.5f, 8);
		}
		break;
	}
	}
#endif
}
