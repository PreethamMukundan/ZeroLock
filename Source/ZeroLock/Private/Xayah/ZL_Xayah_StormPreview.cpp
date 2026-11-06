// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_StormPreview.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Xayah/ZL_Xayah_ThrowAbility.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Xayah_StormPreview::AZL_Xayah_StormPreview()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	CircleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CircleMesh"));
	SetRootComponent(CircleMesh);
	CircleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CircleMesh->SetCastShadow(false);

	// Placeholder until Xayah has real art.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		CircleMesh->SetStaticMesh(CylinderMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		CircleMesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AZL_Xayah_StormPreview::BeginPlay()
{
	Super::BeginPlay();

	CircleMaterial = CircleMesh->CreateDynamicMaterialInstance(0);
	if (CircleMaterial)
	{
		CircleMaterial->SetVectorParameterValue(TEXT("Color"), CircleColor);
	}
}

void AZL_Xayah_StormPreview::InitPreview(float InConeHalfAngle, float InMaxRange)
{
	ConeHalfAngle = InConeHalfAngle;
	MaxRange = InMaxRange;
}

void AZL_Xayah_StormPreview::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (!IsValid(Hero))
	{
		Destroy();
		return;
	}

	// Blades die at MaxRange, so the circle stops there too.
	const FVector HeroLocation = Hero->GetActorLocation();
	const FVector ToAim = (UZL_Xayah_ThrowAbility::TraceAimPoint(Hero) - HeroLocation).GetClampedToMaxSize(MaxRange);
	SetActorLocationAndRotation(HeroLocation + ToAim, FRotator::ZeroRotator);

	// The engine cylinder is 100 cm wide and tall.
	const float Radius = ToAim.Size() * FMath::Tan(FMath::DegreesToRadians(ConeHalfAngle));
	const float WidthScale = FMath::Max(Radius, 50.0f) / 50.0f;
	SetActorScale3D(FVector(WidthScale, WidthScale, 0.03f));
}
