// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_GooBall.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Viscous_GooBall::AZL_Viscous_GooBall()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	BallMesh->SetupAttachment(Root);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetGenerateOverlapEvents(false);
	BallMesh->SetCastShadow(true);
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

void AZL_Viscous_GooBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Viscous_GooBall, Hero);
}

void AZL_Viscous_GooBall::BeginPlay()
{
	Super::BeginPlay();

	// Engine Sphere: 100 cm across, centered.
	BallMesh->SetRelativeScale3D(FVector(BallRadius * 2.0f / 100.0f));
	if (UMaterialInstanceDynamic* MID = BallMesh->GetMaterial(0) ? BallMesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		MID->SetVectorParameterValue(TEXT("Color"), BallColor);
	}
	AttachToHero();
}

void AZL_Viscous_GooBall::OnRep_Hero()
{
	AttachToHero();
}

void AZL_Viscous_GooBall::AttachToHero()
{
	if (!IsValid(Hero))
	{
		return;
	}
	// The attachment may already have replicated: still hide the mesh.
	if (GetAttachParentActor() != Hero)
	{
		AttachToActor(Hero, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	if (USkeletalMeshComponent* HeroMesh = Hero->FindComponentByClass<USkeletalMeshComponent>())
	{
		HeroMesh->SetHiddenInGame(true, true);
	}
}

void AZL_Viscous_GooBall::StartBall(AZeroLockCharacter* InHero, float InImpactDamage, TSubclassOf<UGameplayEffect> InStunEffect, float InStunDuration)
{
	if (!HasAuthority() || !IsValid(InHero))
	{
		Destroy();
		return;
	}
	Hero = InHero;
	ImpactDamage = InImpactDamage;
	StunEffect = InStunEffect;
	StunDuration = InStunDuration;
	AttachToHero();
}

void AZL_Viscous_GooBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsValid(Hero))
	{
		return;
	}

	// Roll: spin about the axis across the velocity, by distance / radius (cosmetic, every machine).
	const FVector Velocity = Hero->GetVelocity();
	const FVector Flat(Velocity.X, Velocity.Y, 0.0f);
	if (GetNetMode() != NM_DedicatedServer && Flat.SizeSquared() > 1.0f && BallRadius > 0.0f)
	{
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Flat.GetSafeNormal());
		SpinRotation = FQuat(Axis, Flat.Size() * DeltaSeconds / BallRadius) * SpinRotation;
		BallMesh->SetWorldRotation(SpinRotation);
	}

	if (HasAuthority())
	{
		CheckImpacts();
	}
}

void AZL_Viscous_GooBall::CheckImpacts()
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	if (!HeroASC || Hero->bIsDead || Hero->GetVelocity().SizeSquared() < FMath::Square(MinImpactSpeed))
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GooBallImpact), false, Hero);
	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	GetWorld()->OverlapMultiByObjectType(Overlaps, Hero->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(BallRadius + ImpactReach), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan))
		{
			continue;
		}
		const double Now = GetWorld()->GetTimeSeconds();
		if (const double* LastHit = LastHitTimes.Find(Villan); LastHit && Now - *LastHit < RehitDelay)
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (!VillanASC || VillanASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_MELEE_PARRY) || HeroASC->IsUntouchable(VillanASC))
		{
			continue;
		}

		LastHitTimes.Add(Villan, Now);
		HeroASC->ApplySpiritDamage(VillanASC, ImpactDamage, this);
		bool bStunApplied = false;
		if (StunEffect && !Villan->bIsDead)
		{
			FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(StunEffect, 1.0f, HeroASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				if (StunDuration > 0.0f)
				{
					Spec.Data->SetDuration(StunDuration, true);
				}
				bStunApplied = HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), VillanASC).WasSuccessfullyApplied();
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[GooBall] hit %s: damage %.0f, stun %s (%s, %.2fs), target stunned now: %d"), *Villan->GetName(), ImpactDamage,
			bStunApplied ? TEXT("applied") : TEXT("NOT applied"), *GetNameSafe(StunEffect), StunDuration,
			VillanASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STUN) ? 1 : 0);
	}
}

void AZL_Viscous_GooBall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Hero))
	{
		if (USkeletalMeshComponent* HeroMesh = Hero->FindComponentByClass<USkeletalMeshComponent>())
		{
			HeroMesh->SetHiddenInGame(false, true);
		}
	}
	Super::EndPlay(EndPlayReason);
}
