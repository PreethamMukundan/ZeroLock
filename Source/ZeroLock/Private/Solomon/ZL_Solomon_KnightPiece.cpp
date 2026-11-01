// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_KnightPiece.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseGameplayAbility.h"
#include "GAS/ZL_GameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Solomon/ZL_Solomon_KnightEffect.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Solomon_KnightPiece::AZL_Solomon_KnightPiece()
{
	// Follows the rider after they've moved this frame.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// Deadlock knightbuff: MoveCooldown 10 (longer than the 7 s buff, so one move per cast). A player, not a square:
	// fine grid, no spacing, a body-sized preview tile and standing-room probe. Quick hop.
	MoveDistance = KnightMoveDistance;
	MoveSpeed = 3500.0f;
	MoveCooldown = 10.0f;
	bSnapToBoard = false;
	MinPieceSpacing = 0.0f;
	SquareSize = 100.0f;
	PieceRadius = 40.0f;

	// Placeholder: a small engine Cone over the rider's head (placed in OnRep_Rider).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		PieceMesh->SetStaticMesh(ConeMesh.Object);
	}
	PieceMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.5f));
	PieceColor = FLinearColor(0.35f, 0.75f, 1.0f);

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	BallMesh->SetupAttachment(Root);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetGenerateOverlapEvents(false);
	BallMesh->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BallMesh->SetStaticMesh(SphereMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		BallMesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AZL_Solomon_KnightPiece::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Solomon_KnightPiece, Rider);
	DOREPLIFETIME(AZL_Solomon_KnightPiece, FlightEndTime);
}

void AZL_Solomon_KnightPiece::InitPromotion(AZeroLockCharacter* InRider, float Lifetime, float HopSpeed)
{
	if (!HasAuthority() || !InRider)
	{
		return;
	}
	Rider = InRider;
	if (HopSpeed > 0.0f)
	{
		// Only the server runs the hop, so this needn't replicate.
		MoveSpeed = HopSpeed;
	}
	OnRep_Rider();
	SetLifeSpan(Lifetime);
	Rider->OnDestroyed.AddDynamic(this, &AZL_Solomon_KnightPiece::OnRiderDestroyed);
	RiderDeathHandle = Rider->OnServerPreDeath.AddUObject(this, &AZL_Solomon_KnightPiece::OnRiderPreDeath);
	ForceNetUpdate();
}

void AZL_Solomon_KnightPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && GetWorld())
	{
		// Gone mid-move (life ran out): don't leave the rider flying at the pull's speed.
		if (GetWorld()->GetTimerManager().IsTimerActive(StopTimer))
		{
			StopRider();
		}
		GetWorld()->GetTimerManager().ClearTimer(LegTimer);
		GetWorld()->GetTimerManager().ClearTimer(StopTimer);
	}
	SetBallForm(false);
	if (Rider)
	{
		Rider->OnDestroyed.RemoveDynamic(this, &AZL_Solomon_KnightPiece::OnRiderDestroyed);
		Rider->OnServerPreDeath.Remove(RiderDeathHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AZL_Solomon_KnightPiece::OnRiderDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void AZL_Solomon_KnightPiece::OnRiderPreDeath(AZeroLockCharacter* DeadHero)
{
	Destroy();
}

void AZL_Solomon_KnightPiece::OnRep_Rider()
{
	MoveDistance = IsKing() ? KingMoveDistance : KnightMoveDistance;
	if (Rider)
	{
		AddTickPrerequisiteActor(Rider);
		const UCapsuleComponent* Capsule = Rider->GetCapsuleComponent();
		const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
		const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 40.0f;
		// Engine shapes are 100 cm, centered. Cone scale Z 0.5 = 50 cm.
		PieceMesh->SetRelativeLocation(FVector(0.0f, 0.0f, HalfHeight * 2.0f + MarkerHeight + 25.0f));
		BallMesh->SetRelativeLocation(FVector(0.0f, 0.0f, HalfHeight));
		BallMesh->SetRelativeScale3D(FVector(Radius * 2.0f * BallScale / 100.0f));
		if (UMaterialInstanceDynamic* BallMaterial = BallMesh->CreateDynamicMaterialInstance(0))
		{
			BallMaterial->SetVectorParameterValue(TEXT("Color"), PieceColor);
		}
		SetActorLocation(GetRiderFeet());
	}
}

bool AZL_Solomon_KnightPiece::IsKing() const
{
	return Rider && Rider == GetInstigator();
}

FVector AZL_Solomon_KnightPiece::GetRiderFeet() const
{
	if (!Rider)
	{
		return GetActorLocation();
	}
	const UCapsuleComponent* Capsule = Rider->GetCapsuleComponent();
	return Rider->GetActorLocation() - FVector(0.0f, 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f);
}

FVector AZL_Solomon_KnightPiece::FeetToCenter(const FVector& Feet) const
{
	const UCapsuleComponent* Capsule = Rider ? Rider->GetCapsuleComponent() : nullptr;
	// A little above standing height so the pull ends clear of the floor and the rider drops onto it.
	return Feet + FVector(0.0f, 0.0f, (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f) + 5.0f);
}

FVector AZL_Solomon_KnightPiece::GetBoardLocation() const
{
	return GetRiderFeet();
}

bool AZL_Solomon_KnightPiece::IsMoving() const
{
	return GetServerTime() < FlightEndTime;
}

void AZL_Solomon_KnightPiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Rider)
	{
		SetActorLocation(GetRiderFeet());
	}
	SetBallForm(Rider && IsMoving());
}

void AZL_Solomon_KnightPiece::SetBallForm(bool bBall)
{
	if (bBallForm == bBall)
	{
		return;
	}
	bBallForm = bBall;
	BallMesh->SetVisibility(bBall);
	PieceMesh->SetVisibility(!bBall);
	if (Rider)
	{
		if (USkeletalMeshComponent* RiderMesh = Rider->FindComponentByClass<USkeletalMeshComponent>())
		{
			RiderMesh->SetHiddenInGame(bBall, true);
		}
	}
}

FVector AZL_Solomon_KnightPiece::GetCorner(const FVector& Start, const FVector& End, float Side) const
{
	// Knight's L: the long leg (2) is the straight line turned by atan(1/2), the short leg (1) is square to it.
	const FVector Flat(End.X - Start.X, End.Y - Start.Y, 0.0f);
	const float Length = Flat.Size();
	if (Length < 1.0f)
	{
		return Start;
	}
	const FVector LongDir = Flat.GetSafeNormal().RotateAngleAxis(Side * FMath::RadiansToDegrees(FMath::Atan2(1.0f, 2.0f)), FVector::UpVector);
	FVector Corner = Start + LongDir * (Length * 2.0f / FMath::Sqrt(5.0f));
	Corner.Z = FMath::Max(Start.Z, End.Z) + CornerLift;
	return Corner;
}

void AZL_Solomon_KnightPiece::GetPreviewPath(const FVector& End, TArray<FVector>& OutPath) const
{
	// The side of the L is random (picked by the server), so the preview shows the straight line.
	OutPath = {GetRiderFeet(), End};
}

bool AZL_Solomon_KnightPiece::ServerStartMove(const FVector& Target)
{
	FVector End;
	AZeroLockCharacter* Hero = GetHero();
	if (!HasAuthority() || !Hero || !Rider || Rider->bIsDead || !GetMoveTarget(Target, End))
	{
		return false;
	}
	const FVector Start = GetRiderFeet();
	const FVector Corner = GetCorner(Start, End, FMath::RandBool() ? 1.0f : -1.0f);
	const float Speed = FMath::Max(MoveSpeed, 1.0f);
	const float FirstLeg = FMath::Max(MinLegDuration, FVector::Dist(Start, Corner) / Speed);
	SecondLegDuration = FMath::Max(MinLegDuration, FVector::Dist(Corner, End) / Speed);
	SecondLegTarget = FeetToCenter(End);
	const float Total = FirstLeg + SecondLegDuration;

	// Untouchable for the whole hop, before it starts.
	FGameplayTagContainer Untouchable;
	Untouchable.AddTag(ZerolockGameplayTagsForBinding::TAG_UNTOUCHABLE);
	UZL_Solomon_KnightEffect::Apply(Hero->GetAbilitySystemComponent(), Rider->GetAbilitySystemComponent(), Total + InvulnerableSlack, Untouchable);

	// The rider's own machine runs the pulls (victim events), so they're predicted there like any pull.
	const FVector CornerCenter = FeetToCenter(Corner);
	UBaseGameplayAbility::SendVictimMoveEvent(Hero, Rider, TEXT("Zerolock.Movement.PullTo"), FirstLeg, &CornerCenter);
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(LegTimer, this, &AZL_Solomon_KnightPiece::StartSecondLeg, FirstLeg, false);
	Timers.SetTimer(StopTimer, this, &AZL_Solomon_KnightPiece::StopRider, Total, false);

	FlightEndTime = GetServerTime() + Total;
	NextMoveTime = FlightEndTime + MoveCooldown;
	SetBallForm(true);
	ForceNetUpdate();
	return true;
}

void AZL_Solomon_KnightPiece::StartSecondLeg()
{
	AZeroLockCharacter* Hero = GetHero();
	if (Hero && Rider && !Rider->bIsDead)
	{
		UBaseGameplayAbility::SendVictimMoveEvent(Hero, Rider, TEXT("Zerolock.Movement.PullTo"), SecondLegDuration, &SecondLegTarget);
	}
}

void AZL_Solomon_KnightPiece::StopRider()
{
	AZeroLockCharacter* Hero = GetHero();
	if (Hero && Rider && !Rider->bIsDead)
	{
		// A pull leaves its speed on the rider: a brief zero knockback stops them on the spot, then they fall normally.
		const FVector Zero = FVector::ZeroVector;
		UBaseGameplayAbility::SendVictimMoveEvent(Hero, Rider, TEXT("Zerolock.Movement.Forced"), 0.05f, &Zero);
	}
}
