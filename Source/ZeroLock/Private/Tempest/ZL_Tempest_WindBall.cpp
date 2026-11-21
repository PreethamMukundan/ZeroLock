// Copyright Preetham Mukundan (C) 2026


#include "Tempest/ZL_Tempest_WindBall.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Mover/ZeroMoverComponent.h"
#include "Mover/ZeroMovementData.h"
#include "UObject/ConstructorHelpers.h"
#include "Xayah/ZL_Xayah_ThrowAbility.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Tempest_WindBall::AZL_Tempest_WindBall(const FObjectInitializer& ObjectInitializer)
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
	HitboxComp->SetCapsuleSize(35.0f, 35.0f);

	ProjectileMovement->InitialSpeed = 1800.0f;
	ProjectileMovement->MaxSpeed = 1800.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bShouldBounce = false;

	InitialLifeSpan = 3.0f;
	bDetonateOnLifeSpanEnd = true;
	// The ball is big and slow: it leaves from the hand, not from beside the gun.
	VisualBlendDistance = 0.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	ShellMesh->SetupAttachment(CollisionComp);
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellMesh->SetCastShadow(false);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(ShellMesh);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetCastShadow(false);

	if (Sphere.Succeeded())
	{
		ShellMesh->SetStaticMesh(Sphere.Object);
		CoreMesh->SetStaticMesh(Sphere.Object);
	}
}

void AZL_Tempest_WindBall::BeginPlay()
{
	Super::BeginPlay();

	// The root (CollisionComp) may carry a scale on BP copies; the shell is sized in world terms.
	const FVector RootScale = CollisionComp ? CollisionComp->GetComponentScale() : FVector::OneVector;
	ShellMesh->SetRelativeScale3D(FVector(ShellScale) / RootScale.GetMax());
	CoreMesh->SetRelativeScale3D(FVector(CoreScale));
	if (UMaterialInstanceDynamic* Material = ShellMesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), ShellColor);
	}
	if (UMaterialInstanceDynamic* Material = CoreMesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), CoreColor);
	}
}

void AZL_Tempest_WindBall::InitWindBall(float InBurstDamage, float InBurstRadius, const UObject* InDamageSource)
{
	BurstDamage = InBurstDamage;
	BurstRadius = InBurstRadius;
	DamageSource = InDamageSource;
}

FVector AZL_Tempest_WindBall::GetCrosshairPoint(const AZeroLockCharacter* Hero, float Range)
{
	if (Hero->IsLocallyControlled() && Hero->IsPlayerControlled())
	{
		return UZL_Xayah_ThrowAbility::TraceAimPoint(Hero);
	}
	FVector Direction = Hero->GetBaseAimRotation().Vector();
	if (Hero->HasAuthority())
	{
		// The server has the player's look direction from the Mover input.
		if (const UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			if (const FZeroMovementInputs* Inputs = Mover->GetLastInputCmd().InputCollection.FindDataByType<FZeroMovementInputs>())
			{
				Direction = Inputs->LookDir.Vector();
			}
		}
	}
	const FVector Start = Hero->GetPawnViewLocation();
	const FVector End = Start + Direction.GetSafeNormal() * Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZL_Tempest_Crosshair), false, Hero);
	FHitResult Hit;
	return Hero->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
}

void AZL_Tempest_WindBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDetonated)
	{
		return;
	}
	Age += DeltaSeconds;
	if (bSteerToCrosshair && Age >= SteerDelay)
	{
		Steer(DeltaSeconds);
	}
}

void AZL_Tempest_WindBall::Steer(float DeltaSeconds)
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (!Hero || !ProjectileMovement)
	{
		return;
	}
	const float Speed = ProjectileMovement->Velocity.Size();
	if (Speed < 1.0f)
	{
		return;
	}
	const FVector Current = ProjectileMovement->Velocity / Speed;
	const FVector Wanted = (GetCrosshairPoint(Hero, SteerRange) - GetActorLocation()).GetSafeNormal();
	if (Wanted.IsNearlyZero())
	{
		return;
	}
	const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Current, Wanted), -1.0f, 1.0f));
	const float MaxStep = FMath::DegreesToRadians(SteerTurnRate) * DeltaSeconds;
	FVector NewDirection = Wanted;
	if (Angle > MaxStep && Angle > KINDA_SMALL_NUMBER)
	{
		const FQuat Turn = FQuat::FindBetweenNormals(Current, Wanted);
		NewDirection = FQuat::Slerp(FQuat::Identity, Turn, MaxStep / Angle).RotateVector(Current);
	}
	ProjectileMovement->Velocity = NewDirection.GetSafeNormal() * Speed;
	SetActorRotation(NewDirection.Rotation());
}

void AZL_Tempest_WindBall::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only enemies detonate it: allies and the caster are flown through.
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	AZeroLockCharacter* Other = Cast<AZeroLockCharacter>(OtherActor);
	if (Other && (!Hero || Other == Hero || Hero->IsOnSameTeam(Other) || Other->bIsDead))
	{
		return;
	}

	const bool bWasDetonated = bDetonated;
	Super::OnHitboxOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	if (!bWasDetonated && bDetonated && IsServerCopy())
	{
		Burst(GetActorLocation());
	}
}

void AZL_Tempest_WindBall::OnStop(const FHitResult& Hit)
{
	if (!bDetonated && IsServerCopy())
	{
		Burst(Hit.Location);
	}
	Super::OnStop(Hit);
}

void AZL_Tempest_WindBall::LifeSpanExpired()
{
	if (bDetonateOnLifeSpanEnd && !bDetonated && IsServerCopy())
	{
		Burst(CollisionComp->GetComponentLocation());
	}
	Super::LifeSpanExpired();
}

void AZL_Tempest_WindBall::FindVillans(const FVector& Center, float Radius, TArray<AZeroLockCharacter*>& OutVillans) const
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC)
	{
		return;
	}

	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_Tempest_WindBall), false, Hero);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(Radius), QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || OutVillans.Contains(Villan))
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (VillanASC && !HeroASC->IsUntouchable(VillanASC))
		{
			OutVillans.Add(Villan);
		}
	}
}

void AZL_Tempest_WindBall::Burst(const FVector& Location)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (bBurst || !Hero || !Hero->GetMyAbilitySystemComp())
	{
		return;
	}
	bBurst = true;
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	ZLKitCue::Play(HeroASC, ImpactCue, Location, GetVelocity().Rotation(), BurstRadius);

	TArray<AZeroLockCharacter*> Villans;
	FindVillans(Location, BurstRadius, Villans);
	for (AZeroLockCharacter* Villan : Villans)
	{
		HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), BurstDamage, GetDamageSource());
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Location, BurstRadius, 16, Villans.Num() > 0 ? FColor::Red : FColor::Yellow, false, 1.5f);
	}
#endif
}
