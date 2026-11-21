// Copyright Preetham Mukundan (C) 2026


#include "KaiKoa/ZL_KaiKoa_SharkProjectile.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_KaiKoa_SharkProjectile::AZL_KaiKoa_SharkProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root collision: only the environment stops it.
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	// Hitbox: only overlaps heroes (Hero object type = ECC_GameTraceChannel1).
	HitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitboxComp->SetCollisionObjectType(ECC_WorldDynamic);
	HitboxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitboxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	HitboxComp->SetGenerateOverlapEvents(true);
	HitboxComp->SetCapsuleSize(40.0f, 40.0f);

	ProjectileMovement->InitialSpeed = 2200.0f;
	ProjectileMovement->MaxSpeed = 2200.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bShouldBounce = false;

	// 2200 cm/s for 0.8 s: about 17.6 m.
	InitialLifeSpan = 0.8f;
}

void AZL_KaiKoa_SharkProjectile::InitShark(float InSpiritDamage, const UObject* InDamageSource)
{
	SpiritDamage = InSpiritDamage;
	DamageSource = InDamageSource;
}

void AZL_KaiKoa_SharkProjectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Intentionally not calling Super: heroes don't stop or detonate a shark. Damage only on the server's real copy
	// (the client's fake is locally authoritative too, so it's excluded explicitly).
	if (!HasAuthority() || bIsFakeProjectile || bDetonated)
	{
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (!IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Villan->bIsDead)
	{
		return;
	}
	if (Hero->IsOnSameTeam(Villan) || BittenVillans.Contains(Villan))
	{
		return;
	}

	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC || HeroASC->IsUntouchable(VillanASC))
	{
		return;
	}

	BittenVillans.Add(Villan);
	HeroASC->ApplySpiritDamage(VillanASC, SpiritDamage, DamageSource.IsValid() ? DamageSource.Get() : this);
	ZLKitCue::Play(VillanASC, HitHeroCue, Villan->GetActorLocation(), GetVelocity().Rotation());
}
