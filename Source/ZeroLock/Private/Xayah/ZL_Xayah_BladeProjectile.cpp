// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_BladeProjectile.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapon/ZL_ProjectileMovementComponent.h"
#include "Xayah/ZL_Xayah_Feather.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Xayah_BladeProjectile::AZL_Xayah_BladeProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root collision: only stop on the environment (WorldStatic).
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

	InitialLifeSpan = 0.6f;
	bDetonateOnLifeSpanEnd = true;

	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	BladeMesh->SetupAttachment(CollisionComp);
	BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BladeMesh->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	BladeMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.5f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		BladeMesh->SetStaticMesh(ConeMesh.Object);
	}

	FeatherClass = AZL_Xayah_Feather::StaticClass();
}

void AZL_Xayah_BladeProjectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Intentionally not calling Super: the base class detonates on a hit, and blades fly on. Damage is only applied by the
	// server's authoritative projectile; the client's fake is locally authoritative, so it's excluded explicitly.
	if (!HasAuthority() || bIsFakeProjectile || bDetonated)
	{
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (!IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Hero->IsOnSameTeam(Villan) || DamagedVillans.Contains(Villan))
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
	HeroASC->ApplySpiritDamage(VillanASC, SpiritDamage, this);
}

void AZL_Xayah_BladeProjectile::OnStop(const FHitResult& Hit)
{
	if (HasAuthority() && !bIsFakeProjectile && !bDetonated)
	{
		// The end of its life also lands here, with no surface: that feather falls to the floor.
		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
		if (Hit.bBlockingHit)
		{
			AZL_Xayah_Feather::Stick(FeatherClass, Hero, Hit);
		}
		else
		{
			AZL_Xayah_Feather::Drop(FeatherClass, Hero, GetActorLocation());
		}
	}
	Super::OnStop(Hit);
}
