// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_Projectile.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapon/ZL_ProjectileMovementComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Kit_Projectile::AZL_Kit_Projectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root: stops (and detonates) on the environment.
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComp->SetSphereRadius(10.0f);

	// Hitbox: heroes only (Hero object type = ECC_GameTraceChannel1).
	HitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitboxComp->SetCollisionObjectType(ECC_WorldDynamic);
	HitboxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitboxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	HitboxComp->SetGenerateOverlapEvents(true);
	HitboxComp->SetCapsuleSize(30.0f, 30.0f);

	ProjectileMovement->InitialSpeed = 2500.0f;
	ProjectileMovement->MaxSpeed = 2500.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	InitialLifeSpan = 1.5f;
	bDetonateOnLifeSpanEnd = true;
	VisualBlendDistance = 0.0f;

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(CollisionComp);
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetCastShadow(false);
	PlaceholderMesh->SetRelativeScale3D(FVector(0.3f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		PlaceholderMesh->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShape(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicShape.Succeeded())
	{
		PlaceholderMesh->SetMaterial(0, BasicShape.Object);
	}
}

void AZL_Kit_Projectile::BeginPlay()
{
	Super::BeginPlay();
	if (bHidePlaceholder && PlaceholderMesh)
	{
		PlaceholderMesh->SetVisibility(false);
	}
	if (PlaceholderMesh && PlaceholderMesh->GetStaticMesh())
	{
		if (UMaterialInstanceDynamic* Material = PlaceholderMesh->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), PlaceholderColor);
		}
	}
}

void AZL_Kit_Projectile::Arm(const FZL_KitProjectileHit& InHit, float InPower, float InRangeScale, FName InZoneKey)
{
	HitData = InHit;
	Power = InPower;
	RangeScale = InRangeScale;
	ZoneKey = InZoneKey;
}

bool AZL_Kit_Projectile::ShouldHit(AActor* OtherActor) const
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Other = Cast<AZeroLockCharacter>(OtherActor);
	if (!Other)
	{
		// Shootable projectiles and the like: the base decides.
		return true;
	}
	return ZLKit::MatchesTeam(Hero, Other, HitTeam) && !AlreadyHit.Contains(OtherActor);
}

void AZL_Kit_Projectile::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!IsValid(OtherActor) || OtherActor == GetInstigator() || !ShouldHit(OtherActor))
	{
		return;
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Target = Cast<AZeroLockCharacter>(OtherActor);
	const bool bServerCopy = HasAuthority() && !bIsFakeProjectile;

	if (Target && bPierce)
	{
		// Every copy remembers who it went through, so the fake and the server agree on what's left to hit.
		AlreadyHit.Add(OtherActor);
		if (bServerCopy && Hero)
		{
			ZLKit::ApplyPayload(this, Hero, Target, GetActorLocation(), HitData.DirectPayload, Power);
		}
		return;
	}

	const bool bWasDetonated = bDetonated;
	Super::OnHitboxOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	if (bWasDetonated || !bDetonated || !bServerCopy || !Hero || !Target)
	{
		return;
	}
	AlreadyHit.Add(OtherActor);
	ZLKit::ApplyPayload(this, Hero, Target, GetActorLocation(), HitData.DirectPayload, Power);
	Explode(GetActorLocation());
}

void AZL_Kit_Projectile::OnStop(const FHitResult& Hit)
{
	// A boomerang turns around at the first wall instead of detonating.
	if (ReturnAfter > 0.f && !bReturning && !bDetonated)
	{
		StartReturn();
		return;
	}
	if (!bDetonated && HasAuthority() && !bIsFakeProjectile)
	{
		Explode(Hit.Location);
	}
	Super::OnStop(Hit);
}

void AZL_Kit_Projectile::LifeSpanExpired()
{
	if (bDetonateOnLifeSpanEnd && !bDetonated && HasAuthority() && !bIsFakeProjectile)
	{
		Explode(CollisionComp->GetComponentLocation());
	}
	Super::LifeSpanExpired();
}

void AZL_Kit_Projectile::Explode(const FVector& Location)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (!Hero)
	{
		return;
	}
	const FVector Center = bZoneOnFloor ? AZL_Kit_Zone::SnapToFloor(GetWorld(), Location) : Location;
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), ImpactCue, Location, GetVelocity().Rotation(), HitData.AreaRadius * RangeScale);

	if (HitData.AreaRadius > 0.f)
	{
		FZL_KitArea Area;
		Area.Range = HitData.AreaRadius;
		TArray<AZeroLockCharacter*> Villans;
		if (!HitData.AreaPayload.IsEmpty())
		{
			ZLKit::FindHeroes(Hero, Location, FVector::ForwardVector, Area, RangeScale, EZL_KitTeam::Enemies, Villans);
			for (AZeroLockCharacter* Villan : Villans)
			{
				ZLKit::ApplyPayload(this, Hero, Villan, Location, HitData.AreaPayload, Power);
			}
		}
		if (!HitData.AreaAllyPayload.IsEmpty())
		{
			TArray<AZeroLockCharacter*> Allies;
			ZLKit::FindHeroes(Hero, Location, FVector::ForwardVector, Area, RangeScale, EZL_KitTeam::Allies, Allies);
			for (AZeroLockCharacter* Ally : Allies)
			{
				ZLKit::ApplyPayload(this, Hero, Ally, Location, HitData.AreaAllyPayload, Power);
			}
		}
#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			ZLDebugDraw::Sphere(GetWorld(), Location, HitData.AreaRadius * RangeScale, Villans.IsEmpty() ? FColor::Yellow : FColor::Red, 1.5f, 16);
		}
#endif
	}

	if (HitData.Zone.IsValid())
	{
		AZL_Kit_Zone::Spawn(HitData.ZoneClass, Hero, Center, HitData.Zone, Power, RangeScale, ZoneKey);
	}
}

void AZL_Kit_Projectile::StartReturn()
{
	bReturning = true;
	AlreadyHit.Reset();
	// Back through walls: only the hitbox matters on the way home.
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	if (ProjectileMovement)
	{
		ProjectileMovement->SetUpdatedComponent(CollisionComp);
		ProjectileMovement->bShouldBounce = false;
		ProjectileMovement->ProjectileGravityScale = 0.f;
		const FVector Home = GetInstigator() ? GetInstigator()->GetActorLocation() : GetActorLocation();
		ProjectileMovement->Velocity = (Home - GetActorLocation()).GetSafeNormal() * FMath::Max(ProjectileMovement->InitialSpeed, 1000.f);
		ProjectileMovement->UpdateComponentVelocity();
	}
}

void AZL_Kit_Projectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ReturnAfter <= 0.f || bDetonated)
	{
		return;
	}

	Age += DeltaSeconds;
	if (!bReturning && Age >= ReturnAfter)
	{
		StartReturn();
	}
	if (!bReturning || !ProjectileMovement)
	{
		return;
	}

	const APawn* Home = GetInstigator();
	if (!Home)
	{
		return;
	}
	const FVector ToHome = Home->GetActorLocation() - GetActorLocation();
	if (ToHome.Size() < 100.f)
	{
		// Caught: the server ends it for everyone; the fake just hides until then.
		if (HasAuthority() && !bIsFakeProjectile)
		{
			Detonate(false, nullptr, nullptr, GetActorLocation(), FVector::UpVector);
		}
		else
		{
			SetActorHiddenInGame(true);
		}
		return;
	}
	// Steer home each frame: the caster keeps moving.
	ProjectileMovement->Velocity = ToHome.GetSafeNormal() * ProjectileMovement->Velocity.Size();
}
