// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_GooPuddle.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_GE_MoveSlow.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Viscous_GooPuddle::AZL_Viscous_GooPuddle()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	SlowArea = CreateDefaultSubobject<USphereComponent>(TEXT("SlowArea"));
	SetRootComponent(SlowArea);
	SlowArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlowArea->SetCollisionObjectType(ECC_WorldDynamic);
	SlowArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	SlowArea->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	SlowArea->SetGenerateOverlapEvents(true);
	SlowArea->CanCharacterStepUpOn = ECB_No;

	PuddleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PuddleMesh"));
	PuddleMesh->SetupAttachment(SlowArea);
	PuddleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PuddleMesh->SetGenerateOverlapEvents(false);
	PuddleMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PuddleMesh->SetStaticMesh(CylinderMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		PuddleMesh->SetMaterial(0, ShapeMaterial.Object);
	}

	SlowEffect = UZL_GE_MoveSlow::StaticClass();
}

void AZL_Viscous_GooPuddle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Viscous_GooPuddle, PuddleRadius);
}

void AZL_Viscous_GooPuddle::InitPuddle(float InRadius, float InDuration, float InSlowMultiplier)
{
	PuddleRadius = InRadius;
	PuddleDuration = InDuration;
	SlowMultiplier = InSlowMultiplier;
	ApplyRadius();
}

void AZL_Viscous_GooPuddle::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* MID = PuddleMesh->GetMaterial(0) ? PuddleMesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		MID->SetVectorParameterValue(TEXT("Color"), GooColor);
	}
	ApplyRadius();

	if (HasAuthority())
	{
		SlowArea->OnComponentBeginOverlap.AddDynamic(this, &AZL_Viscous_GooPuddle::OnSlowAreaBeginOverlap);
		SlowArea->OnComponentEndOverlap.AddDynamic(this, &AZL_Viscous_GooPuddle::OnSlowAreaEndOverlap);
		SetLifeSpan(FMath::Max(PuddleDuration, 0.1f));

		// Heroes already standing where it lands: begin-overlap only fires for new overlaps after the binding.
		TArray<AActor*> Inside;
		SlowArea->GetOverlappingActors(Inside, AZeroLockCharacter::StaticClass());
		for (AActor* Actor : Inside)
		{
			SlowVillan(Cast<AZeroLockCharacter>(Actor));
		}
	}
}

void AZL_Viscous_GooPuddle::OnRep_PuddleRadius()
{
	ApplyRadius();
}

void AZL_Viscous_GooPuddle::ApplyRadius()
{
	SlowArea->SetSphereRadius(PuddleRadius);
	// Engine Cylinder: 100 cm wide and tall, centered. Flat on the ground, top just above it.
	const float Diameter = PuddleRadius * 2.0f / 100.0f;
	PuddleMesh->SetRelativeScale3D(FVector(Diameter, Diameter, PuddleThickness / 100.0f));
	PuddleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, PuddleThickness * 0.5f));
}

void AZL_Viscous_GooPuddle::OnSlowAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	SlowVillan(Cast<AZeroLockCharacter>(OtherActor));
}

void AZL_Viscous_GooPuddle::OnSlowAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ReleaseVillan(Cast<AZeroLockCharacter>(OtherActor));
}

void AZL_Viscous_GooPuddle::SlowVillan(AZeroLockCharacter* Villan)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	if (!HasAuthority() || !SlowEffect || !IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Villan->bIsDead
		|| Hero->IsOnSameTeam(Villan) || SlowedVillans.Contains(Villan))
	{
		return;
	}

	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC)
	{
		return;
	}

	FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(SlowEffect, 1.0f, Context);
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_MOVE_SPEED_MULTIPLIER, SlowMultiplier);
	Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_SLOWED);
	SlowedVillans.Add(Villan, HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), VillanASC));
}

void AZL_Viscous_GooPuddle::ReleaseVillan(AZeroLockCharacter* Villan)
{
	FActiveGameplayEffectHandle Handle;
	if (!Villan || !SlowedVillans.RemoveAndCopyValue(Villan, Handle))
	{
		return;
	}
	if (UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp())
	{
		VillanASC->RemoveActiveGameplayEffect(Handle);
	}
}

void AZL_Viscous_GooPuddle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The puddle is gone: nobody stays slowed by it.
	if (HasAuthority())
	{
		for (const TPair<TWeakObjectPtr<AZeroLockCharacter>, FActiveGameplayEffectHandle>& Pair : SlowedVillans)
		{
			AZeroLockCharacter* Villan = Pair.Key.Get();
			if (UBaseCharAbilitySystemComponent* VillanASC = Villan ? Villan->GetMyAbilitySystemComp() : nullptr)
			{
				VillanASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
		SlowedVillans.Empty();
	}
	Super::EndPlay(EndPlayReason);
}
