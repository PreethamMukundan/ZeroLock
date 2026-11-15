// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_GroundAbility.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Kit_GroundAbility::UZL_Kit_GroundAbility()
{
	SurfaceMode = EZL_SurfaceTargetMode::Ground;
}

void UZL_Kit_GroundAbility::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && Hero->HasAuthority())
	{
		ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), SelfPayload, PowerScale.GetValueAtLevel(GetAbilityLevel()));

		const FVector Location = Hit.ImpactPoint;
		if (ImpactDelay <= 0.0f)
		{
			DoImpact(Location);
		}
		else
		{
			// The ability ends now; the impact belongs to the hero.
			TWeakObjectPtr<UZL_Kit_GroundAbility> WeakThis(this);
			FTimerHandle Handle;
			Hero->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Hero, [WeakThis, Location]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->DoImpact(Location);
				}
			}), ImpactDelay, false);
		}
	}
	Super::PlacementConfirmed(Hit, Placement, Data);
}

void UZL_Kit_GroundAbility::DoImpact(const FVector& Location)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || Hero->bIsDead)
	{
		return;
	}
	const float Power = PowerScale.GetValueAtLevel(GetAbilityLevel());
	const float RangeScale = GetAbilityRangeScale();
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), ImpactCue, Location, FRotator::ZeroRotator, (ImpactRadius > 0.0f ? ImpactRadius : Zone.Radius) * RangeScale);

	if (ImpactRadius > 0.0f)
	{
		FZL_KitArea Area;
		Area.Range = ImpactRadius;
		Area.bRequireLineOfSight = false;
		const FVector Origin = Location + FVector(0, 0, 60);
		TArray<AZeroLockCharacter*> Villans;
		ZLKit::FindHeroes(Hero, Origin, FVector::ForwardVector, Area, RangeScale, EZL_KitTeam::Enemies, Villans);
		for (AZeroLockCharacter* Villan : Villans)
		{
			ZLKit::ApplyPayload(this, Hero, Villan, Location, ImpactEnemyPayload, Power);
		}
		if (!ImpactAllyPayload.IsEmpty())
		{
			TArray<AZeroLockCharacter*> Allies;
			ZLKit::FindHeroes(Hero, Origin, FVector::ForwardVector, Area, RangeScale, EZL_KitTeam::Allies, Allies);
			for (AZeroLockCharacter* Ally : Allies)
			{
				ZLKit::ApplyPayload(this, Hero, Ally, Location, ImpactAllyPayload, Power);
			}
		}
		if (bDrawDebug)
		{
			ZLKit::DrawArea(GetWorld(), Origin, FVector::ForwardVector, Area, RangeScale, Villans.IsEmpty() ? FColor::Yellow : FColor::Red);
		}
	}

	if (Zone.IsValid())
	{
		AZL_Kit_Zone::Spawn(ZoneClass, Hero, Location, Zone, Power, RangeScale, GetClass()->GetFName());
	}
}
