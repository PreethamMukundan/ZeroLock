// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_Feather.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Xayah/ZL_Xayah_Bladecaller.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Xayah_Feather::AZL_Xayah_Feather()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	FeatherMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FeatherMesh"));
	SetRootComponent(FeatherMesh);
	FeatherMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FeatherMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.45f));

	// Placeholder until Xayah has real art.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		FeatherMesh->SetStaticMesh(ConeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		FeatherMesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AZL_Xayah_Feather::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Xayah_Feather, bReturning);
}

void AZL_Xayah_Feather::BeginPlay()
{
	Super::BeginPlay();

	FeatherMaterial = FeatherMesh->CreateDynamicMaterialInstance(0);
	if (FeatherMaterial)
	{
		FeatherMaterial->SetVectorParameterValue(TEXT("Color"), FeatherColor);
	}

	if (HasAuthority())
	{
		SetLifeSpan(GroundLifetime);
	}
}

AZL_Xayah_Feather* AZL_Xayah_Feather::Drop(TSubclassOf<AZL_Xayah_Feather> FeatherClass, AZeroLockCharacter* Hero, const FVector& Location)
{
	UWorld* World = Hero ? Hero->GetWorld() : nullptr;
	if (!World || !FeatherClass || !Hero->HasAuthority())
	{
		return nullptr;
	}

	const AZL_Xayah_Feather* Defaults = FeatherClass->GetDefaultObject<AZL_Xayah_Feather>();
	FVector Spot = Location;
	FHitResult Floor;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Hero);
	if (World->LineTraceSingleByObjectType(Floor, Location + FVector::UpVector * 50.0f, Location - FVector::UpVector * 2000.0f,
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Spot = Floor.ImpactPoint + FVector::UpVector * Defaults->RestHeight;
	}
	return SpawnFeather(FeatherClass, Hero, Spot, FRotator::ZeroRotator);
}

AZL_Xayah_Feather* AZL_Xayah_Feather::Stick(TSubclassOf<AZL_Xayah_Feather> FeatherClass, AZeroLockCharacter* Hero, const FHitResult& Surface)
{
	if (!FeatherClass)
	{
		return nullptr;
	}
	// The placeholder cone's tip is its +Z.
	const float Offset = FeatherClass->GetDefaultObject<AZL_Xayah_Feather>()->RestHeight;
	return SpawnFeather(FeatherClass, Hero, Surface.ImpactPoint + Surface.ImpactNormal * Offset, FRotationMatrix::MakeFromZ(-Surface.ImpactNormal).Rotator());
}

AZL_Xayah_Feather* AZL_Xayah_Feather::SpawnFeather(TSubclassOf<AZL_Xayah_Feather> FeatherClass, AZeroLockCharacter* Hero, const FVector& Location, const FRotator& Rotation)
{
	UWorld* World = Hero ? Hero->GetWorld() : nullptr;
	if (!World || !FeatherClass || !Hero->HasAuthority())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Hero;
	SpawnParams.Instigator = Hero;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AZL_Xayah_Feather>(FeatherClass, Location, Rotation, SpawnParams);
}

TArray<AZL_Xayah_Feather*> AZL_Xayah_Feather::GetGroundFeathers(const AActor* Hero)
{
	TArray<AZL_Xayah_Feather*> Feathers;
	UWorld* World = Hero ? Hero->GetWorld() : nullptr;
	if (!World)
	{
		return Feathers;
	}
	for (TActorIterator<AZL_Xayah_Feather> It(World); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed() && !It->bReturning && It->GetInstigator() == Hero)
		{
			Feathers.Add(*It);
		}
	}
	return Feathers;
}

void AZL_Xayah_Feather::StartReturn(UZL_Xayah_Bladecaller* Recall)
{
	if (!HasAuthority() || bReturning)
	{
		return;
	}
	ReturnRecall = Recall;
	bReturning = true;
	SetLifeSpan(0.0f);
	OnRep_Returning();
}

void AZL_Xayah_Feather::OnRep_Returning()
{
	SetActorTickEnabled(bReturning);
}

void AZL_Xayah_Feather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AActor* Hero = GetInstigator();
	if (!IsValid(Hero))
	{
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}

	const FVector Start = GetActorLocation();
	const FVector ToHero = Hero->GetActorLocation() - Start;
	const float Distance = ToHero.Size();
	const float Step = ReturnSpeed * DeltaSeconds;
	const bool bArrived = Distance <= Step;
	const FVector End = bArrived ? Hero->GetActorLocation() : Start + ToHero / Distance * Step;

	SetActorLocation(End);

	if (HasAuthority())
	{
		HitEnemiesAlong(Start, End);
		if (bArrived)
		{
			Destroy();
		}
	}
}

void AZL_Xayah_Feather::HitEnemiesAlong(const FVector& Start, const FVector& End)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	UZL_Xayah_Bladecaller* Recall = ReturnRecall.Get();
	if (!IsValid(Hero) || !Recall)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // "Hero"
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Hero);

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(ReturnHitRadius), Params);
	for (const FHitResult& Hit : Hits)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Hit.GetActor());
		if (!IsValid(Villan) || Hero->IsOnSameTeam(Villan) || HitVillans.Contains(Villan))
		{
			continue;
		}
		HitVillans.Add(Villan);
		Recall->OnFeatherHit(Hero, Villan);
	}
}
