// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_SuitcaseActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Mover/ZeroMoverPawn.h"
#include "UObject/ConstructorHelpers.h"

AZL_Pocket_SuitcaseActor::AZL_Pocket_SuitcaseActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Every machine attaches the suitcase to the hero itself, so only its existence needs to replicate.
	bReplicates = true;
	SetReplicatingMovement(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SuitcaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SuitcaseMesh"));
	SuitcaseMesh->SetupAttachment(RootComponent);
	SuitcaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SuitcaseMesh->SetGenerateOverlapEvents(false);

	// Placeholder: an 80 x 30 x 60 box. Swap the mesh in a Blueprint subclass.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		SuitcaseMesh->SetStaticMesh(CubeMesh.Object);
	}
	SuitcaseMesh->SetRelativeScale3D(FVector(0.8f, 0.3f, 0.6f));
}

bool AZL_Pocket_SuitcaseActor::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// The owning client spawns its own (predicted) suitcase, so it never needs the server's copy.
	const APawn* Hero = Cast<APawn>(GetOwner());
	if (Hero && !Hero->IsLocallyControlled() && RealViewer && RealViewer == Hero->GetController())
	{
		return false;
	}
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void AZL_Pocket_SuitcaseActor::BeginPlay()
{
	Super::BeginPlay();
	DisguiseOwner();
}

void AZL_Pocket_SuitcaseActor::OnRep_Owner()
{
	Super::OnRep_Owner();

	// The owner usually arrives with the initial bunch (handled in BeginPlay); this covers it arriving later.
	if (HasActorBegunPlay())
	{
		DisguiseOwner();
	}
}

void AZL_Pocket_SuitcaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RevealOwner();
	Super::EndPlay(EndPlayReason);
}

void AZL_Pocket_SuitcaseActor::DisguiseOwner()
{
	APawn* Hero = Cast<APawn>(GetOwner());
	if (!IsValid(Hero) || DisguisedHero.Get() == Hero)
	{
		return;
	}
	DisguisedHero = Hero;

	AttachToActor(Hero, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// The hero's origin is the capsule center; sit the suitcase on the ground instead.
	SuitcaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, SuitcaseMesh->Bounds.BoxExtent.Z - Hero->GetSimpleCollisionHalfHeight()));

	if (const AZeroMoverPawn* MoverPawn = Cast<AZeroMoverPawn>(Hero))
	{
		if (USkeletalMeshComponent* HeroMesh = MoverPawn->GetSkeletalMeshComponent())
		{
			HeroMesh->SetVisibility(false, true);
		}
	}
}

void AZL_Pocket_SuitcaseActor::RevealOwner()
{
	const AZeroMoverPawn* MoverPawn = Cast<AZeroMoverPawn>(DisguisedHero.Get());
	DisguisedHero.Reset();

	if (IsValid(MoverPawn))
	{
		if (USkeletalMeshComponent* HeroMesh = MoverPawn->GetSkeletalMeshComponent())
		{
			HeroMesh->SetVisibility(true, true);
		}
	}
}
