// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Projectile_Viscous_Splatter.h"

#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Viscous/ZL_Viscous_GooPuddle.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Projectile_Viscous_Splatter::AZL_Projectile_Viscous_Splatter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Root collision: bounce off the environment (WorldStatic) only.
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComp->InitSphereRadius(20.0f);

	// Passes through heroes: no hitbox.
	HitboxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitboxComp->SetGenerateOverlapEvents(false);

	GooMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GooMesh"));
	GooMesh->SetupAttachment(RootComponent);
	GooMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GooMesh->SetGenerateOverlapEvents(false);
	GooMesh->SetRelativeScale3D(FVector(0.4f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		GooMesh->SetStaticMesh(SphereMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		GooMesh->SetMaterial(0, ShapeMaterial.Object);
	}

	// Lobbed and bouncy. The bounce count comes from the ability (GooBounces).
	ProjectileMovement->InitialSpeed = 1600.0f;
	ProjectileMovement->MaxSpeed = 1600.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.5f;
	ProjectileMovement->Friction = 0.3f;
	bLimitBounces = true;
	MaximumBounces = GooBounces;
	bStraightenProjectileOnBounce = false;

	PuddleClass = AZL_Viscous_GooPuddle::StaticClass();
	InitialLifeSpan = 6.0f;
}

void AZL_Projectile_Viscous_Splatter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Projectile_Viscous_Splatter, GooBounces);
}

void AZL_Projectile_Viscous_Splatter::BeginPlay()
{
	Super::BeginPlay();

	if (GooMesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* MID = GooMesh->CreateDynamicMaterialInstance(0);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), GooColor);
		}
	}
	ApplyBounceLimit();
}

void AZL_Projectile_Viscous_Splatter::SetGooBounces(int32 InBounces)
{
	GooBounces = FMath::Max(0, InBounces);
	ApplyBounceLimit();
}

void AZL_Projectile_Viscous_Splatter::OnRep_GooBounces()
{
	ApplyBounceLimit();
}

void AZL_Projectile_Viscous_Splatter::ApplyBounceLimit()
{
	MaximumBounces = GooBounces;
	// Only before the limit is reached: OnBounce turns bouncing off at the last one.
	if (BounceCount < GooBounces)
	{
		ProjectileMovement->bShouldBounce = true;
	}
	else
	{
		ProjectileMovement->bShouldBounce = false;
	}
}

void AZL_Projectile_Viscous_Splatter::ArmSplatter(float InImpactDamage, float InImpactRadius, float InPuddleRadius, float InPuddleDuration, float InSlowMultiplier)
{
	ImpactDamage = InImpactDamage;
	ImpactRadius = InImpactRadius;
	PuddleRadius = InPuddleRadius;
	PuddleDuration = InPuddleDuration;
	SlowMultiplier = InSlowMultiplier;
}

void AZL_Projectile_Viscous_Splatter::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	Splat(ImpactResult);
	Super::OnBounce(ImpactResult, ImpactVelocity);
}

void AZL_Projectile_Viscous_Splatter::OnStop(const FHitResult& Hit)
{
	Splat(Hit);
	Super::OnStop(Hit);
}

void AZL_Projectile_Viscous_Splatter::Splat(const FHitResult& Hit)
{
	if (!IsAuthSplatter())
	{
		return;
	}

	const FVector Origin = Hit.ImpactPoint + Hit.ImpactNormal * 10.0f;
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	UBaseCharAbilitySystemComponent* HeroASC = IsValid(Hero) ? Hero->GetMyAbilitySystemComp() : nullptr;

	if (HeroASC && ImpactRadius > 0.0f && ImpactDamage > 0.0f)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SplatterImpact), false, this);
		TArray<FOverlapResult> Overlaps;
		// Heroes use the Hero object type (ECC_GameTraceChannel1).
		GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
			FCollisionShape::MakeSphere(ImpactRadius), Params);

		TSet<AZeroLockCharacter*> HitVillans;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
			if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || HitVillans.Contains(Villan))
			{
				continue;
			}

			// No damage through walls.
			FCollisionQueryParams LosParams(SCENE_QUERY_STAT(SplatterImpactLOS), false, this);
			LosParams.AddIgnoredActor(Villan);
			if (GetWorld()->LineTraceTestByObjectType(Origin, Villan->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), LosParams))
			{
				continue;
			}

			HitVillans.Add(Villan);
			if (UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp())
			{
				HeroASC->ApplySpiritDamage(VillanASC, ImpactDamage, this);
			}
		}
	}

	if (PuddleClass && Hit.ImpactNormal.Z >= FMath::Cos(FMath::DegreesToRadians(PuddleFloorAngle)))
	{
		const FTransform PuddleTransform(FRotationMatrix::MakeFromZ(Hit.ImpactNormal).ToQuat(), Hit.ImpactPoint);
		AZL_Viscous_GooPuddle* Puddle = GetWorld()->SpawnActorDeferred<AZL_Viscous_GooPuddle>(PuddleClass, PuddleTransform, Hero, Hero,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Puddle)
		{
			Puddle->InitPuddle(PuddleRadius, PuddleDuration, SlowMultiplier);
			Puddle->FinishSpawning(PuddleTransform);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, ImpactRadius, 16, FColor::Green, false, 2.0f);
	}
#endif
}
