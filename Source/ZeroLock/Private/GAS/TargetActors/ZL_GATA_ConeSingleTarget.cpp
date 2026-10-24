// Copyright Preetham Mukundan (C) 2026


#include "GAS/TargetActors/ZL_GATA_ConeSingleTarget.h"

#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Abilities/GameplayAbilityWorldReticle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "GAS/Base_GA_TargetActors.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_GATA_ConeSingleTarget::AZL_GATA_ConeSingleTarget()
{
	MaxRange = 1500.0f;

	if (!RootComponent)
	{
		SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
	}

	IndicatorComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IndicatorComponent"));
	IndicatorComponent->SetupAttachment(RootComponent);
	IndicatorComponent->SetUsingAbsoluteLocation(true);
	IndicatorComponent->SetUsingAbsoluteRotation(true);
	IndicatorComponent->SetUsingAbsoluteScale(true);
	IndicatorComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IndicatorComponent->SetGenerateOverlapEvents(false);
	IndicatorComponent->SetCastShadow(false);
	IndicatorComponent->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		IndicatorMesh = CylinderMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		IndicatorMaterial = ShapeMaterial.Object;
	}
}

bool AZL_GATA_ConeSingleTarget::IsValidConeTarget(const AZeroLockCharacter* Hero, const AZeroLockCharacter* Target, const FVector& Origin,
	const FVector& AimDir, float Range, float HalfAngleDeg, EZL_ConeTargetTeam Team, float* OutAngleDeg)
{
	if (!Hero || !IsValid(Target) || Target == Hero || Target->bIsDead || !Target->GetAbilitySystemComponent())
	{
		return false;
	}

	const bool bSameTeam = const_cast<AZeroLockCharacter*>(Hero)->IsOnSameTeam(const_cast<AZeroLockCharacter*>(Target));
	if ((Team == EZL_ConeTargetTeam::Enemies && bSameTeam) || (Team == EZL_ConeTargetTeam::Allies && !bSameTeam))
	{
		return false;
	}

	const FVector ToTarget = Target->GetActorLocation() - Origin;
	if (ToTarget.SizeSquared() > FMath::Square(Range))
	{
		return false;
	}

	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToTarget.GetSafeNormal(), AimDir.GetSafeNormal()), -1.0f, 1.0f)));
	if (AngleDeg > HalfAngleDeg)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZL_GATA_ConeSingleTarget), false, Hero);
	Params.AddIgnoredActor(Target);
	FHitResult Blocker;
	if (Hero->GetWorld()->LineTraceSingleByObjectType(Blocker, Origin, Target->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}

	if (OutAngleDeg)
	{
		*OutAngleDeg = AngleDeg;
	}
	return true;
}

FHitResult AZL_GATA_ConeSingleTarget::PerformTrace(AActor* InSourceActor)
{
	FHitResult HitResult;
	CurrentTarget = nullptr;

	// Only the aiming player picks; the server gets the result as target data.
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(InSourceActor);
	const UBaseGameplayAbility* Ability = Cast<UBaseGameplayAbility>(OwningAbility);
	if (!Hero || !Ability || !PrimaryPC || !PrimaryPC->IsLocalController())
	{
		UpdateIndicator();
		return HitResult;
	}

	// Self-cast: the target data names the caster
	if (bConfirmOnSelf)
	{
		CurrentTarget = Hero;
		HitResult = FHitResult(Hero, nullptr, Hero->GetActorLocation(), FVector::UpVector);
		HitResult.bBlockingHit = true;
		HitResult.TraceStart = Hero->GetActorLocation();
		HitResult.TraceEnd = Hero->GetActorLocation();
		return HitResult;
	}

	const FVector Origin = Hero->GetPawnViewLocation();
	const FVector AimDir = Ability->GetHeroAimDirection();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_GATA_ConeSingleTarget), false, Hero);
	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeSphere(MaxRange), QueryParams);

	float BestAngle = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Candidate = Cast<AZeroLockCharacter>(Overlap.GetActor());
		float Angle = 0.0f;
		if (Candidate && IsValidConeTarget(Hero, Candidate, Origin, AimDir, MaxRange, ConeHalfAngle, TargetTeam, &Angle)
			&& Angle < BestAngle && (!TargetCheck || TargetCheck(Candidate)))
		{
			BestAngle = Angle;
			CurrentTarget = Candidate;
		}
	}

	if (AZeroLockCharacter* Target = CurrentTarget.Get())
	{
		HitResult = FHitResult(Target, nullptr, Target->GetActorLocation(), -AimDir);
		HitResult.bBlockingHit = true;
		HitResult.TraceStart = Origin;
		HitResult.TraceEnd = Target->GetActorLocation();
	}
	else
	{
		HitResult.TraceStart = Origin;
		HitResult.TraceEnd = Origin + AimDir * MaxRange;
	}

	UpdateIndicator();

	if (AGameplayAbilityWorldReticle* Reticle = ReticleActor.Get())
	{
		Reticle->SetIsTargetValid(CurrentTarget.IsValid());
		Reticle->SetActorLocation(HitResult.TraceEnd);
	}

#if ENABLE_DRAW_DEBUG
	if (bDebug)
	{
		const FColor Color = CurrentTarget.IsValid() ? FColor::Green : FColor::Red;
		DrawDebugCone(GetWorld(), Origin, AimDir, MaxRange, FMath::DegreesToRadians(ConeHalfAngle), FMath::DegreesToRadians(ConeHalfAngle), 16, Color, false);
	}
#endif

	return HitResult;
}

void AZL_GATA_ConeSingleTarget::UpdateIndicator()
{
	const AZeroLockCharacter* Target = CurrentTarget.Get();
	const bool bShow = Target && IndicatorMesh;
	IndicatorComponent->SetHiddenInGame(!bShow);
	if (!bShow)
	{
		return;
	}

	if (IndicatorComponent->GetStaticMesh() != IndicatorMesh)
	{
		IndicatorComponent->SetStaticMesh(IndicatorMesh);
	}
	if (!IndicatorMID && IndicatorMaterial)
	{
		IndicatorMID = UMaterialInstanceDynamic::Create(IndicatorMaterial, this);
		IndicatorMID->SetVectorParameterValue(TEXT("Color"), IndicatorColor);
		IndicatorComponent->SetMaterial(0, IndicatorMID);
	}

	const FVector Feet = Target->GetActorLocation() - FVector(0.0f, 0.0f, Target->GetSimpleCollisionHalfHeight() - IndicatorThickness);
	const float Scale = IndicatorRadius * 2.0f / 100.0f;
	IndicatorComponent->SetWorldLocationAndRotation(Feet, FQuat::Identity);
	IndicatorComponent->SetWorldScale3D(FVector(Scale, Scale, IndicatorThickness / 100.0f));
}

void AZL_GATA_ConeSingleTarget::ConfirmTargeting()
{
	if (SourceActor)
	{
		PerformTrace(SourceActor);
	}

	if (CurrentTarget.IsValid())
	{
		Super::ConfirmTargeting();
		return;
	}

	if (bSelfCastWithoutTarget)
	{
		ConfirmOnSelf();
		return;
	}

	// Instant confirms once and Quick confirms on release: neither gets another try
	const UBase_GA_TargetActors* TargetAbility = Cast<UBase_GA_TargetActors>(OwningAbility);
	if (TargetAbility && TargetAbility->TargetConfirmationStyle != EGASTargetConfirmationStyle::Confirm)
	{
		CancelTargeting();
		return;
	}

	// LocalInputConfirm clears its callbacks before broadcasting: bind again so the next press is heard
	if (GenericDelegateBoundASC)
	{
		GenericDelegateBoundASC->GenericLocalConfirmCallbacks.AddUniqueDynamic(this, &AZL_GATA_ConeSingleTarget::ConfirmTargeting);
	}
}

void AZL_GATA_ConeSingleTarget::ConfirmOnSelf()
{
	if (!SourceActor || !ShouldProduceTargetData())
	{
		return;
	}
	bConfirmOnSelf = true;
	Super::ConfirmTargeting();
}
