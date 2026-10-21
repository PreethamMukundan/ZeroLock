// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Projectile_Doorman_LuggageCart.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Doorman/ZL_Doorman_Door.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseGameplayAbility.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapon/ZL_ProjectileMovementComponent.h"
#include "Weapon/ZL_ProjectilePortal.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Projectile_Doorman_LuggageCart::AZL_Projectile_Doorman_LuggageCart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The cart moves itself (MoveCart); the root never blocks anything.
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Hitbox: picks up heroes (Hero channel = ECC_GameTraceChannel1). Sized to CaptureRadius in OnConstruction.
	HitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitboxComp->SetCollisionObjectType(ECC_WorldDynamic);
	HitboxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitboxComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	HitboxComp->SetGenerateOverlapEvents(true);

	// Cart body: 110 long, 70 wide, 60 tall, bottom near the ground (the root sits CartRadius above it).
	CartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartMesh"));
	CartMesh->SetupAttachment(CollisionComp);
	CartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CartMesh->SetGenerateOverlapEvents(false);
	CartMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
	CartMesh->SetRelativeScale3D(FVector(1.1f, 0.7f, 0.6f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		CartMesh->SetStaticMesh(CubeMesh.Object);
	}

	// Enemy capsule centers end up here: just past the front of the cart, standing height.
	DragPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DragPoint"));
	DragPoint->SetupAttachment(CollisionComp);
	DragPoint->SetRelativeLocation(FVector(110.0f, 0.0f, 50.0f));

	static ConstructorHelpers::FClassFinder<UGameplayEffect> InfiniteStun(TEXT("/Game/ZERO/BP/BaseGameplayAbility/BPGE_Stun_Infinite"));
	if (InfiniteStun.Succeeded())
	{
		CarryEffect = InfiniteStun.Class;
	}

	// ProjectileMovement stays inactive. InitialSpeed is still read by proxies to time their detonation.
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;

	// Safety net; the cart normally detonates on a wall or after MaxTravelDistance.
	InitialLifeSpan = 6.0f;
}

void AZL_Projectile_Doorman_LuggageCart::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	HitboxComp->SetCapsuleSize(CaptureRadius, CaptureRadius);
}

void AZL_Projectile_Doorman_LuggageCart::InitCart(float InSpiritDamage, TSubclassOf<UGameplayEffect> InStunEffect)
{
	SpiritDamage = InSpiritDamage;
	if (InStunEffect)
	{
		StunEffect = InStunEffect;
	}
}

void AZL_Projectile_Doorman_LuggageCart::BeginPlay()
{
	Super::BeginPlay();

	// No ProjectileMovement motion (the spawn task fast-forwards it with this velocity).
	ProjectileMovement->Velocity = FVector::ZeroVector;

	// Drive along the flat spawn heading (proxies were just rewound to the spawn transform).
	Heading = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.0f).GetSafeNormal();
	if (Heading.IsNearlyZero())
	{
		Heading = FVector::ForwardVector;
	}

	if (IsAuthCart())
	{
		GetWorldTimerManager().SetTimer(DragTimer, this, &AZL_Projectile_Doorman_LuggageCart::UpdateDrag, DragUpdateInterval, true);
	}
}

void AZL_Projectile_Doorman_LuggageCart::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsAuthCart())
	{
		ReleaseDragged();
	}
	GetWorldTimerManager().ClearTimer(DragTimer);
	Super::EndPlay(EndPlayReason);
}

AZeroLockCharacter* AZL_Projectile_Doorman_LuggageCart::GetHero() const
{
	return Cast<AZeroLockCharacter>(GetInstigator());
}

bool AZL_Projectile_Doorman_LuggageCart::IsWalkable(const FVector& Normal) const
{
	return Normal.Z >= FMath::Cos(FMath::DegreesToRadians(MaxWalkableSlopeAngle));
}

bool AZL_Projectile_Doorman_LuggageCart::SweepStatic(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LuggageCart), false, this);
	Params.AddIgnoredActor(GetInstigator());
	return GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(CartRadius), Params);
}

void AZL_Projectile_Doorman_LuggageCart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bDetonated && bHasSpawnedFully)
	{
		MoveCart(DeltaSeconds);
	}
}

void AZL_Projectile_Doorman_LuggageCart::MoveCart(float DeltaSeconds)
{
	FVector Location = GetActorLocation();

	// On walkable ground: stick to it and drive along its slope. Otherwise fall.
	FHitResult GroundHit;
	const bool bGrounded = VerticalSpeed <= 0.0f
		&& SweepStatic(Location, Location - FVector::UpVector * GroundSnapDistance, GroundHit)
		&& !GroundHit.bStartPenetrating && IsWalkable(GroundHit.ImpactNormal);

	FVector MoveDir = Heading;
	if (bGrounded)
	{
		Location = GroundHit.Location + GroundHit.ImpactNormal * 1.0f;
		MoveDir = FVector::VectorPlaneProject(Heading, GroundHit.ImpactNormal).GetSafeNormal();
		VerticalSpeed = 0.0f;
	}
	else
	{
		VerticalSpeed -= Gravity * DeltaSeconds;
	}

	FVector NewLocation = Location + MoveDir * Speed * DeltaSeconds + FVector::UpVector * VerticalSpeed * DeltaSeconds;

	FHitResult Hit;
	if (SweepStatic(Location, NewLocation, Hit))
	{
		if (Hit.bStartPenetrating)
		{
			// Started inside a wall (e.g. spawned in one): crash rather than drive through it.
			if (!IsWalkable(Hit.ImpactNormal))
			{
				Crash(Hit);
				return;
			}
		}
		else if (!IsWalkable(Hit.ImpactNormal))
		{
			// A linked Doorway door sends us out of the other one.
			if (TryPortal(Hit, MoveDir))
			{
				return;
			}
			// A curb or stair step: climb it.
			if (!bGrounded || !TryStepUp(Location, NewLocation, NewLocation))
			{
				SetActorLocation(Hit.Location);
				Crash(Hit);
				return;
			}
		}
		else
		{
			// Landed, or reached the foot of a slope: stop there, the next step follows the new ground.
			NewLocation = Hit.Location + Hit.ImpactNormal * 1.0f;
			VerticalSpeed = 0.0f;
		}
	}

	Travelled += FVector(NewLocation - GetActorLocation()).Size2D();
	SetActorLocationAndRotation(NewLocation, MoveDir.Rotation());

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), NewLocation, CartRadius, 10, bGrounded ? FColor::Green : FColor::Yellow, false, 0.1f);
		DrawDebugSphere(GetWorld(), DragPoint->GetComponentLocation(), 15.0f, 8, FColor::Cyan, false, 0.1f);
	}
#endif

	if (Travelled >= MaxTravelDistance)
	{
		// Ran out without hitting a wall: dragged enemies are let go, no stun.
		if (IsAuthCart())
		{
			ReleaseDragged();
		}
		Detonate(false, nullptr, nullptr, NewLocation, FVector::UpVector);
	}
}

bool AZL_Projectile_Doorman_LuggageCart::TryStepUp(const FVector& From, const FVector& To, FVector& OutLocation) const
{
	// Up (no ceiling in the way), forward at that height, and the next step's ground snap puts us on the ledge.
	const FVector Up = FVector::UpVector * MaxStepHeight;
	FHitResult Hit;
	if (SweepStatic(From, From + Up, Hit))
	{
		return false;
	}
	if (SweepStatic(From + Up, To + Up, Hit))
	{
		if (Hit.bStartPenetrating || !IsWalkable(Hit.ImpactNormal))
		{
			return false;
		}
		OutLocation = Hit.Location + Hit.ImpactNormal * 1.0f;
		return true;
	}
	OutLocation = To + Up;
	return true;
}

bool AZL_Projectile_Doorman_LuggageCart::TryPortal(const FHitResult& Hit, const FVector& MoveDir)
{
	const IZL_ProjectilePortal* Portal = Cast<IZL_ProjectilePortal>(Hit.GetActor());
	FVector ExitLocation;
	FVector ExitVelocity;
	if (!Portal || !Portal->GetPortalExit(Hit, MoveDir * Speed, CartRadius, ExitLocation, ExitVelocity))
	{
		return false;
	}

	const FVector ExitHeading = FVector(ExitVelocity.X, ExitVelocity.Y, 0.0f).GetSafeNormal();
	if (!ExitHeading.IsNearlyZero())
	{
		Heading = ExitHeading;
	}
	VerticalSpeed = 0.0f;
	Travelled += FVector(Hit.Location - GetActorLocation()).Size2D();
	SetActorLocationAndRotation(ExitLocation, Heading.Rotation());

	// Keeps the fake/auth position correction from pulling one copy across the map while the other hasn't passed yet.
	if (UZL_ProjectileMovementComponent* Movement = Cast<UZL_ProjectileMovementComponent>(ProjectileMovement))
	{
		Movement->AddPortalTraversal();
	}

	if (IsAuthCart())
	{
		UpdateDrag();
	}
	return true;
}

void AZL_Projectile_Doorman_LuggageCart::GetDragHoldLocations(FVector& OutHold, FVector& OutHoldThroughDoor) const
{
	OutHold = DragPoint->GetComponentLocation();
	OutHoldThroughDoor = OutHold;

	/* The hold point is past a linked door we are about to go through. Enemies can't be pulled across the map, so
	 * pull them into the doorway instead: their own client sends them through like any player walking in. Once
	 * through, they are held at the same point mirrored out of the exit door until the cart follows. */
	FHitResult DoorHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LuggageCartDrag), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(DoorHit, GetActorLocation(), OutHold, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		const AZL_Doorman_Door* Door = Cast<AZL_Doorman_Door>(DoorHit.GetActor());
		if (Door && Door->GetLinkedDoor() && Door->IsPortalSurface(DoorHit.GetComponent()))
		{
			OutHoldThroughDoor = Door->MirrorLocation(OutHold);
			OutHold = DoorHit.ImpactPoint - Door->GetActorForwardVector() * 20.0f;
		}
	}
}

void AZL_Projectile_Doorman_LuggageCart::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Intentionally not calling Super: the cart never detonates on players. Server copy only.
	if (!IsAuthCart() || bDetonated)
	{
		return;
	}

	AZeroLockCharacter* Hero = GetHero();
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);
	if (!IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Hero->IsOnSameTeam(Villan)
		|| Dragged.ContainsByPredicate([Villan](const FDraggedVillan& D) { return D.Villan == Villan; }))
	{
		return;
	}

	// Stunned for as long as the cart carries them: cancels their abilities, no moving, no new abilities.
	FDraggedVillan& Entry = Dragged.AddDefaulted_GetRef();
	Entry.Villan = Villan;
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (HeroASC && VillanASC)
	{
		const FGameplayEffectSpecHandle CarrySpec = CarryEffect ? HeroASC->MakeOutgoingSpec(CarryEffect, 1, HeroASC->MakeEffectContext()) : FGameplayEffectSpecHandle();
		if (CarrySpec.IsValid())
		{
			Entry.CarryHandle = HeroASC->ApplyGameplayEffectSpecToTarget(*CarrySpec.Data.Get(), VillanASC);
		}
		HeroASC->ApplySpiritDamage(VillanASC, SpiritDamage);
	}
	UpdateDrag();
}

void AZL_Projectile_Doorman_LuggageCart::UpdateDrag()
{
	AZeroLockCharacter* Hero = GetHero();
	if (bDetonated || !IsValid(Hero))
	{
		return;
	}

	// Hold dragged enemies just in front of the cart; each pull lasts a little longer than the update interval.
	FVector HoldLocation;
	FVector HoldThroughDoor;
	GetDragHoldLocations(HoldLocation, HoldThroughDoor);
	for (int32 i = Dragged.Num() - 1; i >= 0; --i)
	{
		AZeroLockCharacter* Villan = Dragged[i].Villan.Get();
		if (!IsValid(Villan))
		{
			Dragged.RemoveAt(i);
			continue;
		}
		// Through a door ahead: whichever side of it the enemy is on now.
		const FVector VillanLocation = Villan->GetActorLocation();
		const FVector& Target = FVector::DistSquared(VillanLocation, HoldLocation) <= FVector::DistSquared(VillanLocation, HoldThroughDoor) ? HoldLocation : HoldThroughDoor;
		UBaseGameplayAbility::SendVictimMoveEvent(Hero, Villan, TEXT("Zerolock.Movement.PullTo"), DragUpdateInterval * 1.5f, &Target);
	}
}

void AZL_Projectile_Doorman_LuggageCart::ReleaseDragged()
{
	for (const FDraggedVillan& DraggedVillan : Dragged)
	{
		AZeroLockCharacter* Villan = DraggedVillan.Villan.Get();
		UBaseCharAbilitySystemComponent* VillanASC = IsValid(Villan) ? Villan->GetMyAbilitySystemComp() : nullptr;
		if (VillanASC && DraggedVillan.CarryHandle.IsValid())
		{
			VillanASC->RemoveActiveGameplayEffect(DraggedVillan.CarryHandle);
		}
	}
	Dragged.Reset();
	GetWorldTimerManager().ClearTimer(DragTimer);
}

void AZL_Projectile_Doorman_LuggageCart::Crash(const FHitResult& WallHit)
{
	if (IsAuthCart())
	{
		AZeroLockCharacter* Hero = GetHero();
		UBaseCharAbilitySystemComponent* HeroASC = IsValid(Hero) ? Hero->GetMyAbilitySystemComp() : nullptr;
		if (HeroASC && StunEffect)
		{
			// Slammed into the wall with the cart. Applied before the carry stun comes off, so the stun doesn't blink.
			for (const FDraggedVillan& DraggedVillan : Dragged)
			{
				if (AZeroLockCharacter* Villan = DraggedVillan.Villan.Get())
				{
					HeroASC->ApplyGameplayEffect(Villan->GetMyAbilitySystemComp(), StunEffect, 1);
				}
			}
		}
		ReleaseDragged();
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), WallHit.ImpactPoint, 60.0f, 12, FColor::Red, false, 2.0f);
	}
#endif

	// FX, replication and destruction (every copy detonates where it hits the wall).
	Detonate(false, WallHit.GetActor(), WallHit.GetComponent(), WallHit.ImpactPoint, WallHit.ImpactNormal);
}
