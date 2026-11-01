// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_MovePreview.h"

#include "DrawDebugHelpers.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Solomon/ZL_Solomon_ChessPiece.h"
#include "UObject/ConstructorHelpers.h"

AZL_Solomon_MovePreview::AZL_Solomon_MovePreview()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = false;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	SetRootComponent(MarkerMesh);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetGenerateOverlapEvents(false);
	MarkerMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MarkerMesh->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		MarkerMesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AZL_Solomon_MovePreview::Init(AZL_Solomon_ChessPiece* InPiece, APlayerController* InPC, float InAimDistance)
{
	Piece = InPiece;
	PC = InPC;
	AimDistance = InAimDistance;
	if (!MarkerMaterial && MarkerMesh->GetMaterial(0))
	{
		MarkerMaterial = MarkerMesh->CreateDynamicMaterialInstance(0);
	}
	// Engine Cube: 100 cm, centered. A thin tile a bit smaller than the square.
	const float Size = InPiece ? FMath::Max(InPiece->GetSquareSize() - 20.0f, 10.0f) / 100.0f : 1.0f;
	MarkerMesh->SetWorldScale3D(FVector(Size, Size, 0.04f));
	UpdateMarker();
}

FVector AZL_Solomon_MovePreview::GetAimPoint(const APlayerController* PC, float AimDistance, const AActor* IgnoredActor)
{
	if (!PC || !PC->PlayerCameraManager)
	{
		return IgnoredActor ? IgnoredActor->GetActorLocation() : FVector::ZeroVector;
	}
	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PC->PlayerCameraManager->GetCameraRotation().Vector() * AimDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MovePieceAim), false, IgnoredActor);
	FHitResult Hit;
	if (PC->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint;
	}
	return End;
}

void AZL_Solomon_MovePreview::UpdateMarker()
{
	AZL_Solomon_ChessPiece* PieceActor = Piece.Get();
	if (!PieceActor || !PC.IsValid())
	{
		return;
	}
	const FVector Aim = GetAimPoint(PC.Get(), AimDistance, PC->GetPawn());
	FVector Square;
	const bool bValid = PieceActor->GetMoveTarget(Aim, Square);
	const FVector MarkerLocation = bValid ? Square : Aim;
	SetActorLocation(MarkerLocation + FVector(0.0f, 0.0f, 3.0f));
	SetActorRotation(FRotator::ZeroRotator);
	const FLinearColor Color = bValid ? ValidColor : InvalidColor;
	if (MarkerMaterial)
	{
		MarkerMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
	if (bDrawRange)
	{
		// Move range is a sphere: its circle at the piece's height (debug draw, development builds only).
		DrawDebugCircle(GetWorld(), PieceActor->GetActorLocation() + FVector(0.0f, 0.0f, 5.0f), PieceActor->GetMoveDistance(), 64,
			RangeColor.ToFColor(true), false, -1.0f, 0, 3.0f, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), false);
	}
	if (bDrawPath && bValid)
	{
		// The path the piece will follow (the server builds the same one).
		TArray<FVector> Path;
		PieceActor->GetPreviewPath(Square, Path);
		TArray<FVector_NetQuantize10> Curve;
		for (const FVector& Point : Path)
		{
			Curve.Add(Point);
		}
		const int32 Samples = FMath::Max(2, Path.Num() * 3);
		FVector Previous = AZL_Solomon_ChessPiece::EvaluatePath(Curve, 0.0f);
		for (int32 Index = 1; Index <= Samples; ++Index)
		{
			const FVector Point = AZL_Solomon_ChessPiece::EvaluatePath(Curve, static_cast<float>(Index) / Samples);
			DrawDebugLine(GetWorld(), Previous + FVector(0.0f, 0.0f, 5.0f), Point + FVector(0.0f, 0.0f, 5.0f), Color.ToFColor(true), false, -1.0f, 0, 4.0f);
			Previous = Point;
		}
	}
}

void AZL_Solomon_MovePreview::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMarker();
}
