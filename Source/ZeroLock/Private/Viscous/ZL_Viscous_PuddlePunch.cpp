// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_PuddlePunch.h"

#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Viscous_PuddlePunch::UZL_Viscous_PuddlePunch()
{
	SurfaceMode = EZL_SurfaceTargetMode::Any;
	bShowPreview = true;
}

FBox UZL_Viscous_PuddlePunch::GetPreviewBox() const
{
	if (!bShowPreview)
	{
		return FBox(ForceInit);
	}
	const int32 Level = GetAbilityLevel();
	const float HalfWidth = PunchWidth.GetValueAtLevel(Level) * 0.5f;
	return FBox(FVector(0.0f, -HalfWidth, -HalfWidth), FVector(PunchRange.GetValueAtLevel(Level), HalfWidth, HalfWidth));
}

FVector UZL_Viscous_PuddlePunch::GetKnockbackVelocity(const FVector& Normal, int32 Level) const
{
	// Floor / ceiling: along the normal at its own speed
	if (FMath::Abs(Normal.Z) >= FMath::Cos(FMath::DegreesToRadians(FloorAngle)))
	{
		return Normal * FloorKnockbackSpeed.GetValueAtLevel(Level);
	}
	// Wall: out of it, plus a lift so grounded targets go airborne instead of being stopped by ground friction
	return Normal * KnockbackSpeed.GetValueAtLevel(Level) + FVector::UpVector * WallKnockbackLift.GetValueAtLevel(Level);
}

void UZL_Viscous_PuddlePunch::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero)
	{
		Super::PlacementConfirmed(Hit, Placement, Data);
		return;
	}

	const int32 Level = GetAbilityLevel();
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float Range = PunchRange.GetValueAtLevel(Level);
	const float HalfWidth = PunchWidth.GetValueAtLevel(Level) * 0.5f;
	const FVector Velocity = GetKnockbackVelocity(Normal, Level);

	// Box out of the surface: X along the normal (Placement's rotation), centered half the range out
	const FQuat BoxRotation = Placement.GetRotation();
	const FVector BoxCenter = Hit.ImpactPoint + Normal * (Range * 0.5f);
	const FVector BoxExtent(Range * 0.5f, HalfWidth, HalfWidth);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_Viscous_PuddlePunch), false);
	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	GetWorld()->OverlapMultiByObjectType(Overlaps, BoxCenter, BoxRotation, FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeBox(BoxExtent), QueryParams);

	TArray<AZeroLockCharacter*> Targets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Target = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (IsValid(Target) && !Target->bIsDead && Target->GetMyAbilitySystemComp())
		{
			Targets.AddUnique(Target);
		}
	}

	// Viscous's own launch: predicted on his machine (the server ignores latches for remote-controlled pawns anyway)
	if (Targets.Contains(Hero) && IsLocallyControlled())
	{
		if (UZeroMoverComponent* MoverComp = Hero->FindComponentByClass<UZeroMoverComponent>())
		{
			MoverComp->RequestSafeAbilityMove(Velocity, KnockbackDuration);
		}
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
		const float Damage = PunchDamage.GetValueAtLevel(Level);
		for (AZeroLockCharacter* Target : Targets)
		{
			if (Target == Hero)
			{
				continue;
			}

			UBaseCharAbilitySystemComponent* TargetASC = Target->GetMyAbilitySystemComp();
			// A cubed hero (any team) is only shoved: the cube is untouchable but can be punched around.
			const bool bCubed = TargetASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_CUBED);
			if (!bCubed && !Hero->IsOnSameTeam(Target))
			{
				// Parry or Untouchable: the punch does nothing to them
				if (!HeroASC || TargetASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_MELEE_PARRY) || HeroASC->IsUntouchable(TargetASC))
				{
					continue;
				}
				HeroASC->ApplyMeleeDamage(TargetASC, Damage, this);
			}

			if (!Target->bIsDead)
			{
				SendVictimMoveEvent(Hero, Target, TEXT("Zerolock.Movement.Forced"), KnockbackDuration, &Velocity);
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawTargetingDebug)
	{
		DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, BoxRotation, FColor::Blue, false, 2.0f, 0, 2.0f);
		DrawDebugDirectionalArrow(GetWorld(), Hit.ImpactPoint, Hit.ImpactPoint + Normal * Range, 40.0f, FColor::Blue, false, 2.0f, 0, 3.0f);
	}
#endif

	Super::PlacementConfirmed(Hit, Placement, Data);
}
