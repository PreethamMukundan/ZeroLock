// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_CloakProjectile.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Pocket_CloakProjectile::AZL_Pocket_CloakProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root collision: only stop on the environment (WorldStatic).
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	// Hitbox: only overlap heroes (Hero channel = ECC_GameTraceChannel1).
	HitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitboxComp->SetCollisionObjectType(ECC_WorldDynamic);
	HitboxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitboxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	HitboxComp->SetGenerateOverlapEvents(true);

	/* Slide along walls instead of sticking to them. A "bounce" with no bounciness and no friction strips the velocity
	 * into the wall and keeps the part along it; OnBounce then restores the full speed. */
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.0f;
	ProjectileMovement->Friction = 0.0f;
	ProjectileMovement->bBounceAngleAffectsFriction = false;
	bStraightenProjectileOnBounce = false;
	bLimitBounces = false;

	// Lifetime is owned by the FlyingCloak ability, which destroys the projectile on EndAbility.
	InitialLifeSpan = 0.0f;
}

void AZL_Pocket_CloakProjectile::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	// Intentionally not calling Super: straightening and bounce limits don't apply to sliding.
	if (!bPreserveSpeedOnSlide)
	{
		return;
	}

	const FVector SlideDir = ProjectileMovement->Velocity.GetSafeNormal();
	const FVector ImpactDir = ImpactVelocity.GetSafeNormal();

	// Near head-on hits leave no direction to slide in, so let the projectile stop against the wall.
	if (SlideDir.IsNearlyZero() || FMath::Abs(ImpactDir | ImpactResult.ImpactNormal) > MaxSlideImpactDot)
	{
		ProjectileMovement->Velocity = FVector::ZeroVector;
		return;
	}

	ProjectileMovement->Velocity = SlideDir * ImpactVelocity.Size();
}

void AZL_Pocket_CloakProjectile::OnStop(const FHitResult& Hit)
{
	// Intentionally not calling Super: the base class detonates (and destroys) the projectile on stop.
}

void AZL_Pocket_CloakProjectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Damage is only applied by the server's authoritative projectile. The client's fake projectile is
	// locally authoritative, so it has to be excluded explicitly.
	if (!HasAuthority() || bIsFakeProjectile || bDetonated)
	{
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (!IsValid(Hero) || !IsValid(Villan) || Villan == Hero)
	{
		return;
	}

	if (Hero->IsOnSameTeam(Villan) || DamagedVillans.Contains(Villan))
	{
		return;
	}

	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC)
	{
		return;
	}

	DamagedVillans.Add(Villan);
	HeroASC->ApplySpiritDamage(VillanASC, SpiritDamage);
}
