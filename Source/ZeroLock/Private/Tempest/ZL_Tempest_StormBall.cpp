// Copyright Preetham Mukundan (C) 2026


#include "Tempest/ZL_Tempest_StormBall.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseGameplayAbility.h"
#include "Tempest/ZL_Tempest_Vortex.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Tempest_StormBall::AZL_Tempest_StormBall(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HitboxComp->SetCapsuleSize(120.0f, 120.0f);
	ProjectileMovement->InitialSpeed = 4500.0f;
	ProjectileMovement->MaxSpeed = 4500.0f;
	// 4500 cm/s for 1.5 s: about 67 m.
	InitialLifeSpan = 1.5f;
	ShellScale = 2.0f;
	bSteerToCrosshair = false;
	BurstDamage = 60.0f;
	BurstRadius = 350.0f;
}

void AZL_Tempest_StormBall::InitStormBall(float InDirectDamage, float InBurstDamage, float InBurstRadius, const UObject* InDamageSource)
{
	DirectDamage = InDirectDamage;
	InitWindBall(InBurstDamage, InBurstRadius, InDamageSource);
}

void AZL_Tempest_StormBall::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Intentionally not calling Super: players never stop it.
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (bDetonated || !Hero || !IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || PassedThrough.Contains(Villan))
	{
		return;
	}
	PassedThrough.Add(Villan);
	if (!IsServerCopy())
	{
		return;
	}

	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC || HeroASC->IsUntouchable(VillanASC))
	{
		return;
	}
	HeroASC->ApplySpiritDamage(VillanASC, DirectDamage, GetDamageSource());
	ZLKitCue::Play(VillanASC, HitHeroCue, Villan->GetActorLocation(), GetVelocity().Rotation());
	if (!Villan->bIsDead)
	{
		Burst(Villan->GetActorLocation());
	}
}

void AZL_Tempest_StormBall::Burst(const FVector& Location)
{
	// One burst per enemy passed plus one where it stops (OnStop / LifeSpanExpired run once: both need !bDetonated).
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (!Hero || !Hero->GetMyAbilitySystemComp())
	{
		return;
	}
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	ZLKitCue::Play(HeroASC, ImpactCue, Location, GetVelocity().Rotation(), BurstRadius);

	TArray<AZeroLockCharacter*> Villans;
	FindVillans(Location, BurstRadius, Villans);
	for (AZeroLockCharacter* Villan : Villans)
	{
		HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), BurstDamage, GetDamageSource());
		if (!Villan->bIsDead)
		{
			UBaseGameplayAbility::SendVictimMoveEvent(Hero, Villan, TEXT("Zerolock.Movement.PullTo"), FMath::Max(PullDuration, 0.05f), &Location);
		}
	}
	AZL_Tempest_Vortex::SpawnFor(VortexClass, Hero, Location, GetDamageSource());

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Location, BurstRadius, 16, Villans.Num() > 0 ? FColor::Red : FColor::Yellow, false, 1.5f);
	}
#endif
}
