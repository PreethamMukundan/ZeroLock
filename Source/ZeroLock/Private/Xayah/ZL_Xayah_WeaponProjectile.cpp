// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_WeaponProjectile.h"

#include "Xayah/ZL_Xayah_CleanCut.h"
#include "Xayah/ZL_Xayah_Feather.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Xayah_WeaponProjectile::AZL_Xayah_WeaponProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FeatherClass = AZL_Xayah_Feather::StaticClass();
}

void AZL_Xayah_WeaponProjectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPierceHeroes && OtherActor && OtherActor->IsA<AZeroLockCharacter>())
	{
		// Not calling Super: it detonates on the hit. Damage comes from the server's authoritative bullet only; the client's
		// fake is locally authoritative, so it's excluded explicitly.
		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
		if (!HasAuthority() || bIsFakeProjectile || bDetonated || !IsValid(Hero) || Villan == Hero
			|| Hero->IsOnSameTeam(Villan) || DamagedVillans.Contains(Villan))
		{
			return;
		}

		// The hitbox is wider than the bullet: no hits around a corner (same check as the base class).
		FCollisionQueryParams Params(FName(TEXT("HitboxOverlapTrace")), true, this);
		Params.AddIgnoredActor(Villan);
		const FVector BulletLocation = GetActorLocation();
		const FVector TargetPoint = bFromSweep ? FVector(SweepResult.ImpactPoint) : Villan->GetActorLocation();
		if (GetWorld()->LineTraceTestByChannel(TargetPoint, BulletLocation, ECC_Vehicle, Params))
		{
			return;
		}

		DamagedVillans.Add(Villan);

		// Where the bullet is decides falloff and headshots.
		FHitResult Hit(Villan, OtherComp, BulletLocation, -GetActorForwardVector());
		Hit.bBlockingHit = true;
		ApplyEffectToTarget(true, Villan, Hit);
		return;
	}

	const bool bWasDetonated = bDetonated;
	Super::OnHitboxOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	DropFeatherOnDetonation(bWasDetonated, nullptr);
}

void AZL_Xayah_WeaponProjectile::OnStop(const FHitResult& Hit)
{
	const bool bWasDetonated = bDetonated;
	Super::OnStop(Hit);
	DropFeatherOnDetonation(bWasDetonated, Hit.bBlockingHit ? &Hit : nullptr);
}

void AZL_Xayah_WeaponProjectile::DropFeatherOnDetonation(bool bWasDetonated, const FHitResult* Surface)
{
	if (bWasDetonated || !bDetonated || !HasAuthority() || bIsFakeProjectile)
	{
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (IsValid(Hero) && UZL_Xayah_CleanCut::ConsumeFeatherShot(Hero->GetAbilitySystemComponent()))
	{
		if (Surface)
		{
			AZL_Xayah_Feather::Stick(FeatherClass, Hero, *Surface);
		}
		else
		{
			AZL_Xayah_Feather::Drop(FeatherClass, Hero, GetActorLocation());
		}
	}
}
