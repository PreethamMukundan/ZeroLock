// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_QueenTile.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AZL_Solomon_QueenTile::AZL_Solomon_QueenTile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	SetRootComponent(TileMesh);
	TileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TileMesh->SetGenerateOverlapEvents(false);
	TileMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		TileMesh->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		TileMesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AZL_Solomon_QueenTile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Solomon_QueenTile, SquareSize);
}

void AZL_Solomon_QueenTile::BeginPlay()
{
	Super::BeginPlay();
	OnRep_SquareSize();
	if (UMaterialInstanceDynamic* Material = TileMesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), TileColor);
	}
	if (HasAuthority())
	{
		SetLifeSpan(FMath::Max(TileLifetime, 0.05f));
	}
}

void AZL_Solomon_QueenTile::OnRep_SquareSize()
{
	// Engine Cube is 100 cm with its pivot in the middle (the queen spawns the tile a couple of cm above the floor).
	const float Width = FMath::Max(SquareSize - VisualInset * 2.0f, 10.0f) / 100.0f;
	TileMesh->SetWorldScale3D(FVector(Width, Width, TileThickness / 100.0f));
}
