// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_BarrageProjectile.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Engine/Engine.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Pocket_BarrageProjectile::AZL_Pocket_BarrageProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root collision: stop (and detonate) on the environment (WorldStatic).
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
}

void AZL_Pocket_BarrageProjectile::InitBarrage(float InSpiritDamage, float InImpactRadius, TSubclassOf<UGameplayEffect> InStackEffect, int32 InEffectLevel)
{
	SpiritDamage = InSpiritDamage;
	ImpactRadius = InImpactRadius;
	EffectLevel = InEffectLevel;
	if (InStackEffect)
	{
		StackEffect = InStackEffect;
	}
}

void AZL_Pocket_BarrageProjectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Fly through teammates instead of detonating on them.
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (IsValid(Hero) && IsValid(Villan) && Villan != Hero && Hero->IsOnSameTeam(Villan))
	{
		return;
	}

	Super::OnHitboxOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	ApplyImpact();
}

void AZL_Pocket_BarrageProjectile::OnStop(const FHitResult& Hit)
{
	Super::OnStop(Hit);
	ApplyImpact();
}

void AZL_Pocket_BarrageProjectile::ApplyImpact()
{
	if (!bDetonated || bImpactApplied)
	{
		return;
	}
	bImpactApplied = true;

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (!IsValid(Hero))
	{
		return;
	}

	// Damage is only applied by the server's authoritative projectile. The client's fake projectile is
	// locally authoritative, so it has to be excluded explicitly. Clients still run the query for the debug draw.
	const bool bIsServerImpact = HasAuthority() && !bIsFakeProjectile;
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	if (bIsServerImpact && !HeroASC)
	{
		return;
	}

	const FVector ImpactLocation = GetActorLocation();

	FCollisionQueryParams QueryParams(FName(TEXT("BarrageImpact")), false, this);
	QueryParams.AddIgnoredActor(Hero);

	TArray<FOverlapResult> Overlaps;
	// Heroes use the Hero object type (ECC_GameTraceChannel1), and the Hero profile ignores the Hero trace channel, so query by object type.
	GetWorld()->OverlapMultiByObjectType(Overlaps, ImpactLocation, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeSphere(ImpactRadius), QueryParams);

	TSet<AZeroLockCharacter*> DamagedVillans;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Hero->IsOnSameTeam(Villan) || DamagedVillans.Contains(Villan))
		{
			continue;
		}

		if (bIsServerImpact)
		{
			UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
			if (!VillanASC)
			{
				continue;
			}
			ZLOG("ApplyingDamage");
			HeroASC->ApplySpiritDamage(VillanASC, SpiritDamage);
		}
		DamagedVillans.Add(Villan);
	}

	// Green if the impact hit at least one enemy, red if it missed. On clients this is the local (predicted) result.
	if (bDrawDebugImpact)
	{
		DrawDebugSphere(GetWorld(), ImpactLocation, ImpactRadius, 16, DamagedVillans.Num() > 0 ? FColor::Green : FColor::Red, false, DebugDrawTime);
	}

	if (!bIsServerImpact)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Barrage impact: %d overlaps, %d enemies took %.1f spirit damage (radius %.0f)."), Overlaps.Num(), DamagedVillans.Num(), SpiritDamage, ImpactRadius);

	// One stack per impact, no matter how many enemies it hit.
	if (DamagedVillans.Num() > 0 && StackEffect)
	{
		HeroASC->ApplyGameplayEffect(HeroASC, StackEffect, EffectLevel);
	}
}
