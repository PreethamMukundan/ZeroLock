// Copyright Preetham Mukundan (C) 2026


#include "SCombo/ZL_SCombo_Projectile.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapon/ZL_ProjectileMovementComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_SCombo_Projectile::AZL_SCombo_Projectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root collision: stop (and detonate) on the environment (WorldStatic).
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComp->SetSphereRadius(8.0f);

	// Hitbox: only overlap heroes (Hero channel = ECC_GameTraceChannel1).
	HitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitboxComp->SetCollisionObjectType(ECC_WorldDynamic);
	HitboxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitboxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	HitboxComp->SetGenerateOverlapEvents(true);
	HitboxComp->SetCapsuleSize(35.0f, 35.0f);

	ProjectileMovement->InitialSpeed = 3500.0f;
	ProjectileMovement->MaxSpeed = 3500.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	InitialLifeSpan = 0.8f;
	bDetonateOnLifeSpanEnd = true;

	SlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlashMesh"));
	SlashMesh->SetupAttachment(CollisionComp);
	SlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SlashMesh->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	SlashMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.5f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		SlashMesh->SetStaticMesh(ConeMesh.Object);
	}
}

void AZL_SCombo_Projectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (IsValid(Hero) && IsValid(Villan) && Villan != Hero && Hero->IsOnSameTeam(Villan))
	{
		return;
	}

	const bool bAlreadyDetonated = bDetonated;
	Super::OnHitboxOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	// Damage is only applied by the server's authoritative projectile; the client's fake is locally authoritative.
	if (bAlreadyDetonated || !bDetonated || !HasAuthority() || bIsFakeProjectile || !IsValid(Hero) || !IsValid(Villan) || Villan == Hero)
	{
		return;
	}
	bHitVillan = true;

	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	if (HeroASC)
	{
		HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), SpiritDamage, this);
	}
}

void AZL_SCombo_Projectile::Destroyed()
{
#if ENABLE_DRAW_DEBUG
	if (bDrawProjectileDebug && HasAuthority() && !bIsFakeProjectile)
	{
		ZLDebugDraw::Sphere(GetWorld(), GetActorLocation(), HitboxComp->GetScaledCapsuleRadius(), bHitVillan ? FColor::Red : FColor::Yellow, 2.0f);
	}
#endif

	Super::Destroyed();
}
