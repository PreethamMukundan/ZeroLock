// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_ChessPiece.h"

#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Solomon_ChessPiece::AZL_Solomon_ChessPiece()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PieceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMesh"));
	PieceMesh->SetupAttachment(Root);
	// Doesn't block heroes or shots: it's a marker the abilities read, not a wall.
	PieceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PieceMesh->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		PieceMesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AZL_Solomon_ChessPiece::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Solomon_ChessPiece, Move);
	DOREPLIFETIME(AZL_Solomon_ChessPiece, NextMoveTime);
}

void AZL_Solomon_ChessPiece::BeginPlay()
{
	Super::BeginPlay();

	if (PieceMesh->GetMaterial(0))
	{
		PieceMaterial = PieceMesh->CreateDynamicMaterialInstance(0);
		SetHighlight(EZL_PieceHighlight::None);
	}

	if (HasAuthority())
	{
		if (AZeroLockCharacter* Hero = GetHero())
		{
			Hero->OnDestroyed.AddDynamic(this, &AZL_Solomon_ChessPiece::OnHeroDestroyed);
			PreDeathHandle = Hero->OnServerPreDeath.AddUObject(this, &AZL_Solomon_ChessPiece::OnHeroPreDeath);
		}
	}
}

void AZL_Solomon_ChessPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AZeroLockCharacter* Hero = GetHero())
	{
		Hero->OnDestroyed.RemoveDynamic(this, &AZL_Solomon_ChessPiece::OnHeroDestroyed);
		Hero->OnServerPreDeath.Remove(PreDeathHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AZL_Solomon_ChessPiece::OnHeroDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void AZL_Solomon_ChessPiece::OnHeroPreDeath(AZeroLockCharacter* DeadHero)
{
	Destroy();
}

AZeroLockCharacter* AZL_Solomon_ChessPiece::GetHero() const
{
	return Cast<AZeroLockCharacter>(GetInstigator());
}

TArray<AZL_Solomon_ChessPiece*> AZL_Solomon_ChessPiece::GetPiecesOf(const AActor* Hero, TSubclassOf<AZL_Solomon_ChessPiece> PieceClass)
{
	TArray<AZL_Solomon_ChessPiece*> Pieces;
	UWorld* World = Hero ? Hero->GetWorld() : nullptr;
	if (!World)
	{
		return Pieces;
	}
	for (TActorIterator<AZL_Solomon_ChessPiece> It(World, PieceClass ? *PieceClass : AZL_Solomon_ChessPiece::StaticClass()); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->GetInstigator() == Hero)
		{
			Pieces.Add(*It);
		}
	}
	return Pieces;
}

float AZL_Solomon_ChessPiece::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
}

bool AZL_Solomon_ChessPiece::IsMoving() const
{
	return Move.Duration > 0.0f && GetServerTime() < Move.StartTime + Move.Duration;
}

bool AZL_Solomon_ChessPiece::CanMove() const
{
	return !IsMoving() && GetServerTime() >= NextMoveTime;
}

float AZL_Solomon_ChessPiece::GetMoveCooldownRemaining() const
{
	return FMath::Max(0.0f, NextMoveTime - GetServerTime());
}

FVector AZL_Solomon_ChessPiece::SnapToGrid(const FVector& Location, float InSnapSize)
{
	if (InSnapSize <= 0.0f)
	{
		return Location;
	}
	return FVector(FMath::GridSnap(Location.X, InSnapSize), FMath::GridSnap(Location.Y, InSnapSize), Location.Z);
}

bool AZL_Solomon_ChessPiece::FindPlacementSpot(const AActor* Hero, const FVector& Point, bool bCheckSpacing, FVector& OutFloor) const
{
	UWorld* World = Hero ? Hero->GetWorld() : nullptr;
	OutFloor = Point;
	if (!World)
	{
		return false;
	}
	const bool bBoard = bSnapToBoard && SquareSize > 0.0f;
	const FVector Snapped = SnapToGrid(Point, bBoard ? SquareSize : SnapSize);
	TArray<FVector, TInlineAllocator<9>> Candidates;
	if (bBoard)
	{
		// The square under the aim first, then neighbours close to the aim, nearest first.
		for (int32 X = -1; X <= 1; ++X)
		{
			for (int32 Y = -1; Y <= 1; ++Y)
			{
				const FVector Center = Snapped + FVector(X * SquareSize, Y * SquareSize, 0.0f);
				if (FVector::DistSquared2D(Center, Point) <= FMath::Square(SquareSize))
				{
					Candidates.Add(Center);
				}
			}
		}
		Candidates.Sort([&Point](const FVector& A, const FVector& B)
		{
			return FVector::DistSquared2D(A, Point) < FVector::DistSquared2D(B, Point);
		});
	}
	else
	{
		Candidates.Add(Snapped);
	}
	bool bHasGuess = false;
	for (const FVector& Center : Candidates)
	{
		FVector Floor;
		if (!FindStandableFloor(World, Center, Hero, MaxFloorAngle, Floor, 150.0f, 600.0f))
		{
			continue;
		}
		if (HasStandingRoom(World, Floor, Hero, PieceRadius) && (!bCheckSpacing || IsSpotFree(Hero, Floor, MinPieceSpacing)))
		{
			OutFloor = Floor;
			return true;
		}
		if (!bHasGuess)
		{
			OutFloor = Floor;
			bHasGuess = true;
		}
	}
	return false;
}

bool AZL_Solomon_ChessPiece::IsSpotFree(const AActor* Hero, const FVector& Location, float MinSpacing, const AZL_Solomon_ChessPiece* Ignore)
{
	for (const AZL_Solomon_ChessPiece* Piece : GetPiecesOf(Hero))
	{
		if (Piece == Ignore || !Piece->BlocksBoard())
		{
			continue;
		}
		const FVector Other = Piece->GetBoardLocation();
		if (FMath::Abs(Other.Z - Location.Z) > 200.0f)
		{
			continue;
		}
		// Square footprints: Chebyshev distance, so two pieces' squares never overlap.
		if (FMath::Max(FMath::Abs(Other.X - Location.X), FMath::Abs(Other.Y - Location.Y)) < MinSpacing - 1.0f)
		{
			return false;
		}
	}
	return true;
}

bool AZL_Solomon_ChessPiece::FindStandableFloor(const UWorld* World, const FVector& Point, const AActor* IgnoredActor, float MaxFloorAngle,
	FVector& OutFloor, float SearchUp, float SearchDown)
{
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChessPieceFloor), false, IgnoredActor);
	FHitResult Hit;
	if (!World->LineTraceSingleByObjectType(Hit, Point + FVector(0.0f, 0.0f, SearchUp), Point - FVector(0.0f, 0.0f, SearchDown), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
		|| Hit.bStartPenetrating)
	{
		return false;
	}
	if (Hit.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(MaxFloorAngle)))
	{
		return false;
	}
	OutFloor = Hit.ImpactPoint;
	return true;
}

bool AZL_Solomon_ChessPiece::HasStandingRoom(const UWorld* World, const FVector& Floor, const AActor* IgnoredActor, float Radius)
{
	if (!World)
	{
		return false;
	}
	// Spheres above the floor, so the floor itself (and a 45° slope under them) doesn't count, walls and props do.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChessPieceRoom), false, IgnoredActor);
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	for (const float Height : {60.0f, 140.0f})
	{
		if (World->OverlapAnyTestByObjectType(Floor + FVector(0.0f, 0.0f, Height), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Sphere, Params))
		{
			return false;
		}
	}
	return true;
}

FVector AZL_Solomon_ChessPiece::GetBoardLocation() const
{
	return Move.Duration > 0.0f ? FVector(Move.End) : GetActorLocation();
}

bool AZL_Solomon_ChessPiece::GetMoveTarget(const FVector& Desired, FVector& OutTarget) const
{
	const FVector Origin = GetBoardLocation();
	const FVector ToAim = Desired - Origin;
	// Aiming at the piece itself (its own square): a move in place is allowed.
	const bool bAimAtOwnSquare = FMath::Max(FMath::Abs(ToAim.X), FMath::Abs(ToAim.Y)) <= SquareSize * 0.5f && FMath::Abs(ToAim.Z) <= 200.0f;

	if (bSnapToBoard && SquareSize > 0.0f)
	{
		if (bAimAtOwnSquare)
		{
			OutTarget = Origin;
			return true;
		}
		// Squares of the world board in range, nearest the probe (aim point, or the sphere's edge toward it) first; ties go to
		// the one nearer the piece. The path may cross walls; the end can't be in one.
		const FVector Probe = Origin + ToAim.GetClampedToMaxSize(MoveDistance);
		const int32 Cells = FMath::FloorToInt(MoveDistance / SquareSize);
		const double OriginCellX = FMath::RoundToDouble(Origin.X / SquareSize);
		const double OriginCellY = FMath::RoundToDouble(Origin.Y / SquareSize);
		TArray<FVector, TInlineAllocator<64>> Candidates;
		for (int32 X = -Cells; X <= Cells; ++X)
		{
			for (int32 Y = -Cells; Y <= Cells; ++Y)
			{
				if ((X == 0 && Y == 0) || FMath::Square(X * SquareSize) + FMath::Square(Y * SquareSize) > FMath::Square(MoveDistance))
				{
					continue;
				}
				Candidates.Add(FVector((OriginCellX + X) * SquareSize, (OriginCellY + Y) * SquareSize, Probe.Z));
			}
		}
		Candidates.Sort([&Probe, &Origin](const FVector& A, const FVector& B)
		{
			const double DistA = FVector::DistSquared2D(A, Probe);
			const double DistB = FVector::DistSquared2D(B, Probe);
			return !FMath::IsNearlyEqual(DistA, DistB, 1.0) ? DistA < DistB : FVector::DistSquared2D(A, Origin) < FVector::DistSquared2D(B, Origin);
		});
		for (const FVector& Center : Candidates)
		{
			FVector Floor;
			if (FindStandableFloor(GetWorld(), Center, GetInstigator(), MaxFloorAngle, Floor, 150.0f, 600.0f)
				&& FVector::DistSquared(Floor, Origin) <= FMath::Square(MoveDistance)
				&& HasStandingRoom(GetWorld(), Floor, GetInstigator(), PieceRadius)
				&& IsSpotFree(GetInstigator(), Floor, MinPieceSpacing, this))
			{
				OutTarget = Floor;
				return true;
			}
		}
		return false;
	}

	const FVector Dir = ToAim.GetSafeNormal();
	// The aim point if it's inside the sphere, else the edge of the sphere toward it; then back toward the piece until
	// there's standable ground near the line with room to stand (not in or against a wall). The path may cross walls.
	const float Step = FMath::Max(SnapSize, 35.0f);
	const float Reach = Dir.IsNearlyZero() ? 0.0f : FMath::Min(ToAim.Size(), MoveDistance);
	for (float Distance = Reach; Distance >= Step * 0.5f; Distance -= Step)
	{
		const FVector Probe = Origin + Dir * Distance;
		FVector Floor;
		if (!FindStandableFloor(GetWorld(), SnapToGrid(Probe, SnapSize), GetInstigator(), MaxFloorAngle, Floor, 150.0f, 600.0f))
		{
			continue;
		}
		if (FVector::DistSquared(Floor, Origin) > FMath::Square(MoveDistance + SnapSize * 0.75f)
			|| !HasStandingRoom(GetWorld(), Floor, GetInstigator(), PieceRadius)
			|| !IsSpotFree(GetInstigator(), Floor, MinPieceSpacing, this))
		{
			continue;
		}
		OutTarget = Floor;
		return true;
	}
	if (bAimAtOwnSquare)
	{
		OutTarget = Origin;
		return true;
	}
	return false;
}

void AZL_Solomon_ChessPiece::BuildGroundPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const
{
	const int32 Segments = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(From, To) / FMath::Max(PathSampleSpacing, 10.0f)));
	OutPath.SetNum(Segments + 1);
	TArray<bool> OnGround;
	OnGround.Init(false, Segments + 1);
	OutPath[0] = From;
	OutPath[Segments] = To;
	OnGround[0] = OnGround[Segments] = true;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChessPiecePath), false, this);
	if (const AActor* Hero = GetInstigator())
	{
		Params.AddIgnoredActor(Hero);
	}
	float PreviousZ = From.Z;
	for (int32 Index = 1; Index < Segments; ++Index)
	{
		FVector Point = FMath::Lerp(From, To, static_cast<float>(Index) / Segments);
		FHitResult Hit;
		// From StepUpHeight above the last ground point: a taller obstacle starts the trace inside it (no hit) = bridged.
		if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, PreviousZ + StepUpHeight), FVector(Point.X, Point.Y, PreviousZ - 600.0f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params) && !Hit.bStartPenetrating && Hit.ImpactNormal.Z > 0.3f)
		{
			Point.Z = Hit.ImpactPoint.Z;
			PreviousZ = Point.Z;
			OnGround[Index] = true;
		}
		OutPath[Index] = Point;
	}

	// Bridge each run without ground (gap, tall wall, ledge too high to step onto): straight between the ground points on
	// either side plus an arc, like a hop.
	for (int32 Index = 1; Index < Segments; ++Index)
	{
		if (OnGround[Index])
		{
			continue;
		}
		const int32 First = Index - 1;
		int32 Last = Index;
		while (!OnGround[Last])
		{
			++Last;
		}
		for (int32 Gap = Index; Gap < Last; ++Gap)
		{
			const float T = static_cast<float>(Gap - First) / (Last - First);
			OutPath[Gap].Z = FMath::Lerp(OutPath[First].Z, OutPath[Last].Z, T) + JumpArcHeight * FMath::Sin(PI * T);
		}
		Index = Last;
	}
}

void AZL_Solomon_ChessPiece::GetPreviewPath(const FVector& End, TArray<FVector>& OutPath) const
{
	BuildMovePath(GetActorLocation(), End, OutPath);
}

FVector AZL_Solomon_ChessPiece::EvaluatePath(const TArray<FVector_NetQuantize10>& Path, float Alpha)
{
	if (Path.Num() == 0)
	{
		return FVector::ZeroVector;
	}
	if (Path.Num() == 1)
	{
		return Path[0];
	}
	TArray<float, TInlineAllocator<64>> Lengths;
	float Total = 0.0f;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		Lengths.Add(FVector::Dist(Path[Index - 1], Path[Index]));
		Total += Lengths.Last();
	}
	if (Total <= KINDA_SMALL_NUMBER)
	{
		return Path.Last();
	}

	float Remaining = FMath::Clamp(Alpha, 0.0f, 1.0f) * Total;
	int32 Segment = 0;
	while (Segment < Lengths.Num() - 1 && Remaining > Lengths[Segment])
	{
		Remaining -= Lengths[Segment];
		++Segment;
	}
	const float U = Lengths[Segment] > KINDA_SMALL_NUMBER ? FMath::Clamp(Remaining / Lengths[Segment], 0.0f, 1.0f) : 1.0f;

	// Uniform Catmull-Rom through the neighbours. The samples are evenly spaced on a straight top-down line, so X/Y stay on
	// that line; only the height curves.
	const FVector P0 = Path[FMath::Max(Segment - 1, 0)];
	const FVector P1 = Path[Segment];
	const FVector P2 = Path[Segment + 1];
	const FVector P3 = Path[FMath::Min(Segment + 2, Path.Num() - 1)];
	const float U2 = U * U;
	const float U3 = U2 * U;
	return 0.5f * ((2.0f * P1) + (P2 - P0) * U + (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * U2 + (3.0f * P1 - P0 - 3.0f * P2 + P3) * U3);
}

bool AZL_Solomon_ChessPiece::ServerStartMove(const FVector& Target)
{
	FVector End;
	if (!HasAuthority() || !GetMoveTarget(Target, End))
	{
		return false;
	}
	TArray<FVector> Path;
	BuildMovePath(GetActorLocation(), End, Path);
	float Length = 0.0f;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		Length += FVector::Dist(Path[Index - 1], Path[Index]);
	}
	StartMoveAlong(Path, FMath::Max(MinMoveDuration, MoveSpeed > 0.0f ? Length / MoveSpeed : 0.0f));
	return true;
}

void AZL_Solomon_ChessPiece::BuildMovePath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const
{
	BuildGroundPath(From, To, OutPath);
}

void AZL_Solomon_ChessPiece::StartMoveAlong(const TArray<FVector>& Path, float Duration)
{
	if (!HasAuthority() || Path.Num() < 2)
	{
		return;
	}
	const FVector Start = Path[0];
	const FVector End = Path.Last();
	Move.Start = Start;
	Move.End = End;
	Move.Path.Reset(Path.Num());
	for (const FVector& Point : Path)
	{
		Move.Path.Add(Point);
	}
	Move.StartTime = GetServerTime();
	Move.Duration = FMath::Max(Duration, 0.01f);
	++Move.MoveId;
	NextMoveTime = Move.StartTime + Move.Duration + MoveCooldown;
	bMoveInProgress = true;

	const FVector Dir = (End - Start).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
	OnMoveStarted();
	ForceNetUpdate();
}

void AZL_Solomon_ChessPiece::OnRep_Move()
{
	const FVector Dir = (FVector(Move.End) - FVector(Move.Start)).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
}

void AZL_Solomon_ChessPiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Move.Duration <= 0.0f)
	{
		return;
	}
	const float Alpha = FMath::Clamp((GetServerTime() - Move.StartTime) / Move.Duration, 0.0f, 1.0f);
	const FVector Previous = GetActorLocation();
	const FVector Current = Move.Path.Num() >= 2 ? EvaluatePath(Move.Path, Alpha) : FMath::Lerp(FVector(Move.Start), FVector(Move.End), Alpha);
	if (!Previous.Equals(Current, 0.1f))
	{
		SetActorLocation(Current);
	}

	if (HasAuthority() && bMoveInProgress)
	{
		OnMoveStep(Previous, Current);
		if (Alpha >= 1.0f)
		{
			bMoveInProgress = false;
			OnMoveFinished();
		}
	}
}

void AZL_Solomon_ChessPiece::SetHighlight(EZL_PieceHighlight InHighlight)
{
	Highlight = InHighlight;
	if (PieceMaterial)
	{
		const FLinearColor Color = Highlight == EZL_PieceHighlight::Selected ? SelectedColor
			: Highlight == EZL_PieceHighlight::Hover ? HoverColor : PieceColor;
		PieceMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
