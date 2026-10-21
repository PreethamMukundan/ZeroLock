// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_HotelPlaceholder.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMeshComponent* CreatePlaceholderMesh(AActor* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(false);
		return Comp;
	}
}

AZL_Doorman_HotelPlaceholder::AZL_Doorman_HotelPlaceholder()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BodyMesh = CreatePlaceholderMesh(this, TEXT("BodyMesh"), RootComponent, CylinderMesh.Object);
	SignMesh = CreatePlaceholderMesh(this, TEXT("SignMesh"), RootComponent, CubeMesh.Object);
	if (ShapeMaterial.Succeeded())
	{
		Material = ShapeMaterial.Object;
	}
}

void AZL_Doorman_HotelPlaceholder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Engine Cylinder and Cube are 100 cm and centered on their origin.
	BodyMesh->SetRelativeScale3D(FVector(BodyRadius * 2.0f, BodyRadius * 2.0f, BodyHalfHeight * 2.0f) / 100.0f);
	SignMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BodyHalfHeight + 40.0f));
	SignMesh->SetRelativeScale3D(FVector(0.1f, 0.8f, 0.4f));
}

void AZL_Doorman_HotelPlaceholder::BeginPlay()
{
	Super::BeginPlay();

	if (Material && GetNetMode() != NM_DedicatedServer)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		BodyMesh->SetMaterial(0, MID);
		SignMesh->SetMaterial(0, MID);
	}
}
