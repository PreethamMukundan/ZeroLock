// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_BishopSquare.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Solomon_BishopSquare::AZL_Solomon_BishopSquare()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	TriggerBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->CanCharacterStepUpOn = ECB_No;

	SquareMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SquareMesh"));
	SquareMesh->SetupAttachment(TriggerBox);
	SquareMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SquareMesh->SetGenerateOverlapEvents(false);
	SquareMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		SquareMesh->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		SquareMesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AZL_Solomon_BishopSquare::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Solomon_BishopSquare, SquareSize);
}

void AZL_Solomon_BishopSquare::InitSquare(float InSize, float InHeight, float InDamage)
{
	SquareSize = InSize;
	SquareHeight = InHeight;
	SquareDamage = InDamage;
	ApplySize();
}

void AZL_Solomon_BishopSquare::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* MID = SquareMesh->GetMaterial(0) ? SquareMesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		MID->SetVectorParameterValue(TEXT("Color"), SquareColor);
	}
	ApplySize();

	if (HasAuthority())
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AZL_Solomon_BishopSquare::OnTriggerBeginOverlap);

		// An enemy already standing there when it appears steps "in" at once.
		TArray<AActor*> Inside;
		TriggerBox->GetOverlappingActors(Inside, AZeroLockCharacter::StaticClass());
		for (AActor* Actor : Inside)
		{
			TryTrigger(Cast<AZeroLockCharacter>(Actor));
		}
	}
}

void AZL_Solomon_BishopSquare::OnRep_Size()
{
	ApplySize();
}

void AZL_Solomon_BishopSquare::ApplySize()
{
	// The actor is the box center, spawned SquareHeight / 2 above the floor (AZL_Solomon_Bishop::SpawnSquares).
	const float Half = SquareSize * 0.5f;
	TriggerBox->SetBoxExtent(FVector(Half, Half, SquareHeight * 0.5f));
	// Engine Cube: 100 cm, centered. Flat tile just above the floor at the bottom of the box.
	const float Visual = FMath::Max(SquareSize - VisualInset, 1.0f) / 100.0f;
	SquareMesh->SetRelativeScale3D(FVector(Visual, Visual, SquareThickness / 100.0f));
	SquareMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -SquareHeight * 0.5f + SquareThickness * 0.5f + 1.0f));
}

void AZL_Solomon_BishopSquare::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryTrigger(Cast<AZeroLockCharacter>(OtherActor));
}

void AZL_Solomon_BishopSquare::TryTrigger(AZeroLockCharacter* Villan)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (bTriggered || !HasAuthority() || !IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan))
	{
		return;
	}
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC)
	{
		return;
	}
	bTriggered = true;
	HeroASC->ApplySpiritDamage(VillanASC, SquareDamage, this);
	Destroy();
}
