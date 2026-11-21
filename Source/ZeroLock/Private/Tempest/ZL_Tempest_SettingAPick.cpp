// Copyright Preetham Mukundan (C) 2026


#include "Tempest/ZL_Tempest_SettingAPick.h"

#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Items/ZL_GE_ItemStats.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Tempest_SettingAPick::UZL_Tempest_SettingAPick()
{
	SurfaceMode = EZL_SurfaceTargetMode::Ground;
	MaxPlacementRange = 1500.0f;
	AbilityInputID = EGASAbilityInputID::Ability_2;
	Slot = EGameplayAbilitySlot::AbilitySlot3;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_TEMPEST_SETTINGAPICK);
	CooldownDuration = 14.0f;
	AbilityName = TEXT("Setting a Pick");
	AbilityDescription = TEXT("A gust at the target spot yanks nearby enemies to its centre, deals 30 spirit damage and slows them by 30% for 1.5 s. Sets up Alley-oop.");
}

float UZL_Tempest_SettingAPick::GetPreviewDiscRadius() const
{
	return PullRadius.GetValueAtLevel(GetAbilityLevel()) * GetAbilityRangeScale();
}

void UZL_Tempest_SettingAPick::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && HasAuthority(&CurrentActivationInfo))
	{
		const FVector Location = Hit.ImpactPoint;
		if (ImpactDelay <= 0.0f)
		{
			Gust(Location);
		}
		else
		{
			// The ability ends now; the gust belongs to the hero's timers.
			TWeakObjectPtr<UZL_Tempest_SettingAPick> WeakThis(this);
			FTimerHandle Handle;
			Hero->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Hero, [WeakThis, Location]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->Gust(Location);
				}
			}), ImpactDelay, false);
		}
	}
	Super::PlacementConfirmed(Hit, Placement, Data);
}

void UZL_Tempest_SettingAPick::Gust(const FVector& Location)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC || Hero->bIsDead)
	{
		return;
	}

	const int32 Level = GetAbilityLevel();
	const float Radius = PullRadius.GetValueAtLevel(Level) * GetAbilityRangeScale();
	ZLKitCue::Play(HeroASC, BurstCue, Location, FRotator::ZeroRotator, Radius);

	// Heroes stand on the spot: measure from about chest height. Heroes use the Hero object type (ECC_GameTraceChannel1).
	const FVector Origin = Location + FVector(0.0f, 0.0f, 60.0f);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_Tempest_SettingAPick), false, Hero);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(Radius), QueryParams);

	const float Damage = SpiritDamage.GetValueAtLevel(Level);
	const float Slow = SlowPercent.GetValueAtLevel(Level);
	const float SlowTime = SlowDuration.GetValueAtLevel(Level);
	TSet<AZeroLockCharacter*> Pulled;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || Pulled.Contains(Villan))
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (!VillanASC || HeroASC->IsUntouchable(VillanASC))
		{
			continue;
		}
		Pulled.Add(Villan);

		HeroASC->ApplySpiritDamage(VillanASC, Damage, this);
		if (Villan->bIsDead)
		{
			continue;
		}
		SendVictimMoveEvent(Hero, Villan, TEXT("Zerolock.Movement.PullTo"), FMath::Max(PullDuration, 0.05f), &Location);

		if (Slow > 0.0f && SlowTime > 0.0f)
		{
			FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
			Context.SetAbility(this);
			const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(UZL_GE_ItemStatsTimed::StaticClass(), 1.0f, Context);
			if (Spec.IsValid())
			{
				FZL_ItemStatValue SlowStat;
				SlowStat.Stat = EZL_ItemStat::MoveSlowPercent;
				SlowStat.Value = -Slow;
				Spec.Data->SetDuration(SlowTime, true);
				UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, { SlowStat });
				HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, VillanASC);
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, Radius, 16, Pulled.Num() > 0 ? FColor::Red : FColor::Yellow, false, 1.5f);
	}
#endif
}
