// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_Rook.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Solomon/ZL_Solomon_KnightEffect.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Solomon_Rook::AZL_Solomon_Rook()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Placeholder: an engine Cylinder standing on the floor (the root is RookRadius above it).
	RookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RookMesh"));
	RookMesh->SetupAttachment(Root);
	RookMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RookMesh->SetGenerateOverlapEvents(false);
	RookMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.8f));
	RookMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f - RookRadius));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		RookMesh->SetStaticMesh(CylinderMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		RookMesh->SetMaterial(0, ShapeMaterial.Object);
	}

	// Safety net, set by InitRook from the longest possible trip; it normally ends at its travel distance or a wall.
	InitialLifeSpan = 8.0f;
}

void AZL_Solomon_Rook::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Solomon_Rook, State);
}

void AZL_Solomon_Rook::InitRook(float InDamage, float InImmobilizeDuration, float InSpeed, float InMaxTravelDistance, float InForkDistance,
	int32 InMaxReflects, const FVector& InForkHeading)
{
	Damage = InDamage;
	ImmobilizeDuration = InImmobilizeDuration;
	Speed = InSpeed;
	DistanceLeft = InMaxTravelDistance;
	ForkDistance = InForkDistance;
	ReflectsLeft = FMath::Clamp(InMaxReflects, 0, 255);
	ForkHeading = FVector(InForkHeading.X, InForkHeading.Y, 0.0f).GetSafeNormal();
	// Each bounce can retrace at most a full MaxTravelDistance.
	InitialLifeSpan = (InMaxTravelDistance * (ReflectsLeft + 1) + InForkDistance) / FMath::Max(Speed, 1.0f) + 2.0f;
}

void AZL_Solomon_Rook::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* Material = RookMesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), RookColor);
	}

	if (HasAuthority())
	{
		Heading = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.0f).GetSafeNormal();
		if (Heading.IsNearlyZero())
		{
			Heading = FVector::ForwardVector;
		}
		StartLeg();
		PushState();
	}
}

AZeroLockCharacter* AZL_Solomon_Rook::GetHero() const
{
	return Cast<AZeroLockCharacter>(GetInstigator());
}

float AZL_Solomon_Rook::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
}

void AZL_Solomon_Rook::PushState()
{
	State.Location = GetActorLocation();
	State.Heading = Heading;
	State.ServerTime = GetServerTime();
	State.DistanceLeft = DistanceLeft;
	State.ReflectsLeft = static_cast<uint8>(ReflectsLeft);
	State.bRetracing = IsRetracing();
	++State.Id;
	AppliedStateId = State.Id;
	ForceNetUpdate();
}

void AZL_Solomon_Rook::OnRep_State()
{
	if (State.Id == AppliedStateId)
	{
		return;
	}
	AppliedStateId = State.Id;

	Heading = FVector(State.Heading.X, State.Heading.Y, 0.0f).GetSafeNormal();
	DistanceLeft = State.DistanceLeft;
	ReflectsLeft = State.ReflectsLeft;
	VerticalSpeed = 0.0f;
	bFinished = false;
	RookMesh->SetVisibility(true);
	SetActorLocationAndRotation(State.Location, Heading.Rotation());

	if (State.bRetracing)
	{
		// The server bounced at State.Location: walk back along our own copy of the leg (already ours if we bounced first).
		if (!IsRetracing())
		{
			RetracePath = MoveTemp(LegPath);
		}
		LegPath.Reset();
		RetraceIndex = RetracePath.Num() - 1;
	}
	else
	{
		RetracePath.Reset();
		RetraceIndex = INDEX_NONE;
		StartLeg();
	}

	// Catch up with the server: it's been moving since State.ServerTime.
	float Elapsed = FMath::Clamp(GetServerTime() - State.ServerTime, 0.0f, 1.0f);
	while (Elapsed > KINDA_SMALL_NUMBER && !bFinished)
	{
		const float Step = FMath::Min(Elapsed, 1.0f / 60.0f);
		StepRook(Step);
		Elapsed -= Step;
	}
}

void AZL_Solomon_Rook::Fork()
{
	if (!HasAuthority() || State.bForked || bFinished || ForkHeading.IsNearlyZero())
	{
		return;
	}
	Heading = ForkHeading;
	DistanceLeft = ForkDistance;
	RetracePath.Reset();
	RetraceIndex = INDEX_NONE;
	HitThisLeg.Reset();
	State.bForked = true;
	SetActorRotation(Heading.Rotation());
	StartLeg();
	PushState();
}

void AZL_Solomon_Rook::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Clients wait for the server's first state.
	if (bFinished || (!HasAuthority() && AppliedStateId == 0))
	{
		return;
	}
	const FVector From = GetActorLocation();
	StepRook(DeltaSeconds);
	if (HasAuthority())
	{
		SweepHits(From, GetActorLocation());
		if (bFinished)
		{
			Destroy();
		}
	}
}

bool AZL_Solomon_Rook::IsWalkable(const FVector& Normal) const
{
	return Normal.Z >= FMath::Cos(FMath::DegreesToRadians(MaxWalkableSlopeAngle));
}

bool AZL_Solomon_Rook::SweepStatic(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SolomonRook), false, this);
	Params.AddIgnoredActor(GetInstigator());
	return GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(RookRadius), Params);
}

void AZL_Solomon_Rook::StepRook(float DeltaSeconds)
{
	if (IsRetracing())
	{
		StepRetrace(DeltaSeconds);
		return;
	}

	FVector Location = GetActorLocation();

	// Same ground rules as the Luggage Cart: stick to walkable ground and follow its slope, otherwise fall.
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

	// Don't overshoot the travel distance (flat).
	const float FlatStep = FMath::Min(Speed * DeltaSeconds, DistanceLeft);
	const float FlatScale = FMath::Max(FVector(MoveDir.X, MoveDir.Y, 0.0f).Size(), KINDA_SMALL_NUMBER);
	FVector NewLocation = Location + MoveDir * (FlatStep / FlatScale) + FVector::UpVector * VerticalSpeed * DeltaSeconds;

	FHitResult Hit;
	if (SweepStatic(Location, NewLocation, Hit))
	{
		if (Hit.bStartPenetrating)
		{
			if (!IsWalkable(Hit.ImpactNormal))
			{
				Reflect(Location);
				return;
			}
		}
		else if (!IsWalkable(Hit.ImpactNormal))
		{
			if (!bGrounded || !TryStepUp(Location, NewLocation, NewLocation))
			{
				Reflect(Hit.Location);
				return;
			}
		}
		else
		{
			// Landed, or the foot of a slope: the next step follows the new ground.
			NewLocation = Hit.Location + Hit.ImpactNormal * 1.0f;
			VerticalSpeed = 0.0f;
		}
	}

	DistanceLeft -= FVector(NewLocation - GetActorLocation()).Size2D();
	SetActorLocationAndRotation(NewLocation, Heading.Rotation());
	LegPath.Add(NewLocation);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), NewLocation, RookRadius, 8, bGrounded ? FColor::Green : FColor::Yellow, false, 0.1f);
		const FVector Floor = NewLocation - FVector(0.0f, 0.0f, RookRadius);
		DrawDebugCapsule(GetWorld(), Floor + FVector(0.0f, 0.0f, HitHeight * 0.5f), HitHeight * 0.5f, HitRadius, FQuat::Identity, FColor::Blue, false, 0.1f);
	}
#endif

	if (DistanceLeft <= 0.0f || NewLocation.Z < GetWorld()->GetWorldSettings()->KillZ)
	{
		Finish();
	}
}

bool AZL_Solomon_Rook::TryStepUp(const FVector& From, const FVector& To, FVector& OutLocation) const
{
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

void AZL_Solomon_Rook::Reflect(const FVector& WallLocation)
{
	SetActorLocation(WallLocation);
	if (ReflectsLeft <= 0)
	{
		Finish();
		return;
	}
	--ReflectsLeft;
	BeginRetrace(WallLocation);
	if (HasAuthority())
	{
		// Deadlock bAllowMultihitOnReverse: the way back can hit the same enemies (after the immunity).
		HitThisLeg.Reset();
		PushState();
	}
}

void AZL_Solomon_Rook::StartLeg()
{
	LegPath.Reset();
	LegPath.Add(GetActorLocation());
}

void AZL_Solomon_Rook::BeginRetrace(const FVector& WallLocation)
{
	RetracePath = MoveTemp(LegPath);
	LegPath.Reset();
	RetraceIndex = RetracePath.Num() - 1;

	// It always makes it all the way back.
	float Length = 0.0f;
	FVector Previous = WallLocation;
	for (int32 Index = RetraceIndex; Index >= 0; --Index)
	{
		Length += FVector::Dist2D(Previous, RetracePath[Index]);
		Previous = RetracePath[Index];
	}
	DistanceLeft = FMath::Max(DistanceLeft, Length);

	// After the path it carries on the way it came from.
	Heading = -Heading;
	VerticalSpeed = 0.0f;
	SetActorRotation(Heading.Rotation());
	if (!IsRetracing())
	{
		EndRetrace();
	}
}

void AZL_Solomon_Rook::StepRetrace(float DeltaSeconds)
{
	FVector Location = GetActorLocation();
	FVector Facing = Heading;
	float Remaining = Speed * DeltaSeconds;
	while (Remaining > 0.0f && RetraceIndex >= 0)
	{
		const FVector Target = RetracePath[RetraceIndex];
		const FVector ToTarget = Target - Location;
		const float Distance = ToTarget.Size2D();
		if (Distance > KINDA_SMALL_NUMBER)
		{
			Facing = FVector(ToTarget.X, ToTarget.Y, 0.0f) / Distance;
		}
		if (Distance <= Remaining)
		{
			Location = Target;
			Remaining -= Distance;
			DistanceLeft -= Distance;
			--RetraceIndex;
		}
		else
		{
			Location += ToTarget * (Remaining / Distance);
			DistanceLeft -= Remaining;
			Remaining = 0.0f;
		}
	}
	SetActorLocationAndRotation(Location, Facing.Rotation());

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Location, RookRadius, 8, FColor::Cyan, false, 0.1f);
	}
#endif

	if (RetraceIndex < 0)
	{
		EndRetrace();
	}
}

void AZL_Solomon_Rook::EndRetrace()
{
	RetracePath.Reset();
	RetraceIndex = INDEX_NONE;
	SetActorRotation(Heading.Rotation());
	StartLeg();
	if (DistanceLeft <= 1.0f)
	{
		Finish();
	}
}

void AZL_Solomon_Rook::Finish()
{
	bFinished = true;
	// Clients hide it until the server's copy is destroyed (or a newer state brings it back).
	if (!HasAuthority())
	{
		RookMesh->SetVisibility(false);
	}
}

void AZL_Solomon_Rook::SweepHits(const FVector& From, const FVector& To)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}
	const float HalfHeight = FMath::Max(HitHeight * 0.5f, HitRadius);
	const FVector Offset(0.0f, 0.0f, HalfHeight - RookRadius);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SolomonRookHit), false, this);
	Params.AddIgnoredActor(Hero);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, From + Offset, To + Offset + FVector(0.0f, 0.0f, 0.01f), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeCapsule(HitRadius, HalfHeight), Params);
	for (const FHitResult& Hit : Hits)
	{
		HitVillan(Cast<AZeroLockCharacter>(Hit.GetActor()));
	}
}

bool AZL_Solomon_Rook::IsImmune(const AZeroLockCharacter* Villan, float Now) const
{
	const float* Last = LastHitTimes.Find(Villan);
	return Last && Now - *Last < ImmunityDuration;
}

void AZL_Solomon_Rook::HitVillan(AZeroLockCharacter* Villan)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || HitThisLeg.Contains(Villan))
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (IsImmune(Villan, Now) || (Partner.IsValid() && Partner->IsImmune(Villan, Now)))
	{
		return;
	}
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC || HeroASC->IsUntouchable(VillanASC))
	{
		return;
	}
	HitThisLeg.Add(Villan);
	LastHitTimes.Add(Villan, Now);
	HeroASC->ApplySpiritDamage(VillanASC, Damage, this);

	// Immobilize = root (Mover "Locked" while the tag is on).
	FGameplayTagContainer RootTags;
	RootTags.AddTag(ZerolockGameplayTagsForBinding::TAG_MOVEMENT_ROOTED);
	UZL_Solomon_KnightEffect::Apply(HeroASC, VillanASC, ImmobilizeDuration, RootTags);
}
