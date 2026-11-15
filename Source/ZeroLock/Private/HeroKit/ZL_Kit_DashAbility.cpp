// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_DashAbility.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Kit_DashAbility::UZL_Kit_DashAbility()
{
	DashDamage = 0.0f;
}

FVector UZL_Kit_DashAbility::GetDashDirection_Implementation() const
{
	const FVector Aim = GetHeroAimDirection();
	return bFlatDash ? Aim.GetSafeNormal2D() : Aim.GetSafeNormal();
}

void UZL_Kit_DashAbility::OnDashStart_Implementation(const FVector& Direction)
{
	Super::OnDashStart_Implementation(Direction);
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && Hero->HasAuthority())
	{
		ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), SelfPayload, PowerScale.GetValueAtLevel(GetAbilityLevel()));
		ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), DashCue, Hero->GetActorLocation(), Direction.Rotation());
	}
}

void UZL_Kit_DashAbility::OnDashHit_Implementation(AZeroLockCharacter* Villan, const FHitResult& Hit)
{
	// DashDamage still applies through the base if a BP sets it.
	if (DashDamage.GetValueAtLevel(GetAbilityLevel()) > 0.0f)
	{
		Super::OnDashHit_Implementation(Villan, Hit);
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero)
	{
		ZLKit::ApplyPayload(this, Hero, Villan, Hero->GetActorLocation(), HitPayload, PowerScale.GetValueAtLevel(GetAbilityLevel()));
	}
}
