// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Projectile_Doorman_CallBell.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Projectile_Doorman_CallBell::AZL_Projectile_Doorman_CallBell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root collision: only stop on the environment (WorldStatic).
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	// Hitbox: only overlap heroes (Hero channel = ECC_GameTraceChannel1). Sized to ImpactRadius in OnConstruction.
	HitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitboxComp->SetCollisionObjectType(ECC_WorldDynamic);
	HitboxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitboxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	HitboxComp->SetGenerateOverlapEvents(true);

	// Shootable: weapon projectile hitboxes (WorldDynamic) only overlap the Hero object type.
	ShootableComp = CreateDefaultSubobject<USphereComponent>(TEXT("ShootableComponent"));
	ShootableComp->SetupAttachment(RootComponent);
	ShootableComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ShootableComp->SetCollisionObjectType(ECC_GameTraceChannel1);
	ShootableComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShootableComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ShootableComp->SetGenerateOverlapEvents(true);
	ShootableComp->CanCharacterStepUpOn = ECB_No;

	// Slow, straight, no bounce: hitting a wall stops it (OnStop).
	ProjectileMovement->InitialSpeed = 1200.0f;
	ProjectileMovement->MaxSpeed = 1200.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bShouldBounce = false;

	// Lifetime is owned by the fuse.
	InitialLifeSpan = 0.0f;
}

void AZL_Projectile_Doorman_CallBell::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const float Radius = GetImpactRadius();
	HitboxComp->SetCapsuleSize(Radius, Radius);
	ShootableComp->SetSphereRadius(ShootableRadius);
}

float AZL_Projectile_Doorman_CallBell::GetImpactRadius() const
{
	if (ImpactRadius > 0.0f)
	{
		return ImpactRadius;
	}

	// From the mesh asset's bounds and the component's scale (component bounds may not be up to date yet here).
	float MeshRadius = 0.0f;
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		if (const UStaticMesh* Asset = Mesh->GetStaticMesh())
		{
			MeshRadius = FMath::Max(MeshRadius, (Asset->GetBounds().BoxExtent * Mesh->GetRelativeScale3D().GetAbs()).GetMax());
		}
	}
	return MeshRadius > 0.0f ? MeshRadius : 25.0f;
}

void AZL_Projectile_Doorman_CallBell::BeginPlay()
{
	Super::BeginPlay();

	// Always armed with the defaults; ArmBell restarts the fuse with the ability's values.
	if (IsAuthBell() && !FuseTimer.IsValid())
	{
		ArmBell(ImpactDamage, ExplosionDamage, FuseTime);
	}
}

void AZL_Projectile_Doorman_CallBell::ArmBell(float InImpactDamage, float InExplosionDamage, float InFuseTime)
{
	if (!IsAuthBell() || bDetonated)
	{
		return;
	}

	ImpactDamage = InImpactDamage;
	ExplosionDamage = InExplosionDamage;
	FuseTime = InFuseTime;

	GetWorldTimerManager().SetTimer(FuseTimer, this, &AZL_Projectile_Doorman_CallBell::Explode, FMath::Max(FuseTime, 0.01f), false);
}

bool AZL_Projectile_Doorman_CallBell::IsEnemy(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan) const
{
	return IsValid(Hero) && IsValid(Villan) && Villan != Hero && !Hero->IsOnSameTeam(Villan);
}

void AZL_Projectile_Doorman_CallBell::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Intentionally not calling Super: the bell never detonates on overlap.
	if (!IsAuthBell() || bDetonated || (bStopped && !bImpactDamageWhileStopped))
	{
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (!IsEnemy(Hero, Villan) || DamagedVillans.Contains(Villan))
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
	HeroASC->ApplySpiritDamage(VillanASC, ImpactDamage);
}

void AZL_Projectile_Doorman_CallBell::OnStop(const FHitResult& Hit)
{
	// Intentionally not calling Super: the base class detonates on stop.
	bStopped = true;
}

bool AZL_Projectile_Doorman_CallBell::HandleHitByProjectile(AProjectile* OtherProjectile)
{
	if (bDetonated || !IsValid(OtherProjectile) || !OtherProjectile->IsWeaponProjectile()
		|| !GetInstigator() || OtherProjectile->GetInstigator() != GetInstigator())
	{
		return false;
	}

	// Only the server's copies trigger the explosion; fake/proxy copies just let the shot detonate visually.
	if (IsAuthBell() && !OtherProjectile->bIsFakeProjectile)
	{
		Explode();
	}
	return true;
}

void AZL_Projectile_Doorman_CallBell::Explode()
{
	if (!IsAuthBell() || bDetonated)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(FuseTimer);

	const FVector Origin = GetActorLocation();
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	UBaseCharAbilitySystemComponent* HeroASC = IsValid(Hero) ? Hero->GetMyAbilitySystemComp() : nullptr;

	if (HeroASC && ExplosionRadius > 0.0f)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CallBellExplosion), false, this);
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
			FCollisionShape::MakeSphere(ExplosionRadius), Params);

		TSet<AZeroLockCharacter*> HitVillans;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
			if (!IsEnemy(Hero, Villan) || HitVillans.Contains(Villan))
			{
				continue;
			}

			// No damage through walls.
			FCollisionQueryParams LosParams(SCENE_QUERY_STAT(CallBellExplosionLOS), false, this);
			LosParams.AddIgnoredActor(Villan);
			if (GetWorld()->LineTraceTestByObjectType(Origin, Villan->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), LosParams))
			{
				continue;
			}

			HitVillans.Add(Villan);
			if (UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp())
			{
				HeroASC->ApplySpiritDamage(VillanASC, ExplosionDamage);
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, ExplosionRadius, 16, FColor::Orange, false, 2.0f);
	}
#endif

	Detonate(false, nullptr, nullptr, Origin, FVector::UpVector);
}
