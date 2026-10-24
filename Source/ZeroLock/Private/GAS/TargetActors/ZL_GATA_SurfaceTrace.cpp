// Copyright Preetham Mukundan (C) 2026


#include "GAS/TargetActors/ZL_GATA_SurfaceTrace.h"

#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityWorldReticle.h"
#include "GAS/Base_GA_TargetActors.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AZL_GATA_SurfaceTrace::AZL_GATA_SurfaceTrace()
{
	MaxRange = 2000.f;

	if (!RootComponent)
	{
		SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
	}

	// Placement preview: world transform set every aim tick, never collides.
	PreviewComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewComponent"));
	PreviewComponent->SetupAttachment(RootComponent);
	PreviewComponent->SetUsingAbsoluteLocation(true);
	PreviewComponent->SetUsingAbsoluteRotation(true);
	PreviewComponent->SetUsingAbsoluteScale(true);
	PreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewComponent->SetGenerateOverlapEvents(false);
	PreviewComponent->SetCastShadow(false);
	PreviewComponent->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PreviewMesh = CubeMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		PreviewMaterial = ShapeMaterial.Object;
	}
}

bool AZL_GATA_SurfaceTrace::UpdatePreview(bool bHasHit, const FTransform& Placement, bool bValid)
{
	const APlayerController* PC = PrimaryPC;
	const bool bShow = bHasHit && PreviewBox.IsValid && PreviewMesh && PC && PC->IsLocalController();
	PreviewComponent->SetHiddenInGame(!bShow);
	if (!bShow)
	{
		return false;
	}

	if (PreviewComponent->GetStaticMesh() != PreviewMesh)
	{
		PreviewComponent->SetStaticMesh(PreviewMesh);
	}
	if (!PreviewMID && PreviewMaterial)
	{
		PreviewMID = UMaterialInstanceDynamic::Create(PreviewMaterial, this);
		PreviewComponent->SetMaterial(0, PreviewMID);
		bPreviewShowsValid = !bValid; // force the first color
	}
	if (PreviewMID && bPreviewShowsValid != bValid)
	{
		bPreviewShowsValid = bValid;
		PreviewMID->SetVectorParameterValue(TEXT("Color"), bValid ? ValidPreviewColor : InvalidPreviewColor);
	}

	if (!bFitPreviewMeshToBox)
	{
		PreviewComponent->SetWorldTransform(FTransform(Placement.GetRotation(), Placement.GetLocation()));
		return true;
	}

	// Fit the mesh's own bounds onto the box (the engine Cube: 100 cm centered, so scale = size / 100)
	const FBox MeshBounds = PreviewMesh->GetBoundingBox();
	const FVector MeshSize = MeshBounds.GetSize();
	const FVector BoxSize = PreviewBox.GetSize();
	const FVector Scale(MeshSize.X > KINDA_SMALL_NUMBER ? BoxSize.X / MeshSize.X : 1.f,
		MeshSize.Y > KINDA_SMALL_NUMBER ? BoxSize.Y / MeshSize.Y : 1.f,
		MeshSize.Z > KINDA_SMALL_NUMBER ? BoxSize.Z / MeshSize.Z : 1.f);
	const FVector LocalPivot = PreviewBox.GetCenter() - MeshBounds.GetCenter() * Scale;
	PreviewComponent->SetWorldTransform(FTransform(Placement.GetRotation(), Placement.TransformPositionNoScale(LocalPivot), Scale));
	return true;
}

bool AZL_GATA_SurfaceTrace::IsSurfaceAllowed(const FVector& Normal) const
{
	return IsSurfaceAllowedFor(SurfaceMode, MaxGroundAngle, WallAngleTolerance, Normal);
}

bool AZL_GATA_SurfaceTrace::IsSurfaceAllowedFor(EZL_SurfaceTargetMode Mode, float GroundAngle, float WallTolerance, const FVector& Normal)
{
	switch (Mode)
	{
	case EZL_SurfaceTargetMode::Ground:
		return Normal.Z >= FMath::Cos(FMath::DegreesToRadians(GroundAngle));
	case EZL_SurfaceTargetMode::Wall:
		return FMath::Abs(Normal.Z) <= FMath::Sin(FMath::DegreesToRadians(WallTolerance));
	default:
		return true;
	}
}

FHitResult AZL_GATA_SurfaceTrace::PerformTrace(AActor* InSourceActor)
{
	FHitResult HitResult;
	bLastTraceWasValid = false;

	APlayerController* PC = OwningAbility ? OwningAbility->GetCurrentActorInfo()->PlayerController.Get() : nullptr;
	if (!InSourceActor || !PC)
	{
		return HitResult;
	}

	FVector ViewStart;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewStart, ViewRot);
	const FVector ViewDir = ViewRot.Vector();

	// Range is measured from the hero, not the camera
	const FVector HeroLocation = InSourceActor->GetActorLocation();
	FVector ViewEnd = ViewStart + ViewDir * MaxRange;
	ClipCameraRayToAbilityRange(ViewStart, ViewDir, HeroLocation, MaxRange, ViewEnd);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZL_GATA_SurfaceTrace), false);
	Params.AddIgnoredActor(InSourceActor);

	// WorldStatic only: players and other dynamic objects never block the aim
	InSourceActor->GetWorld()->LineTraceSingleByObjectType(HitResult, ViewStart, ViewEnd, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	if (!HitResult.bBlockingHit)
	{
		HitResult.TraceStart = ViewStart;
		HitResult.TraceEnd = ViewEnd;
	}

	bLastTraceWasValid = HitResult.bBlockingHit
		&& FVector::DistSquared(HeroLocation, HitResult.ImpactPoint) <= FMath::Square(MaxRange)
		&& IsSurfaceAllowed(HitResult.ImpactNormal)
		&& (MaxAnchorDistance <= 0.f || FVector::DistSquared(AnchorLocation, HitResult.ImpactPoint) <= FMath::Square(MaxAnchorDistance))
		&& !InvalidActors.Contains(HitResult.GetActor());

	// The owning ability decides where the thing actually goes and whether it fits there.
	FTransform Placement(FRotationMatrix::MakeFromX(HitResult.ImpactNormal).ToQuat(), HitResult.ImpactPoint);
	if (HitResult.bBlockingHit && PlacementCheck && !PlacementCheck(HitResult, Placement))
	{
		bLastTraceWasValid = false;
	}
	if (bLastTraceWasValid && !Clearance.IsClear(GetWorld(), Placement, PreviewBox, InSourceActor))
	{
		bLastTraceWasValid = false;
	}
	// Only on the kind of surface this targets (e.g. Wall mode: nothing while looking at the floor); red there = doesn't fit/out of range.
	const bool bShowingPreview = UpdatePreview(HitResult.bBlockingHit && IsSurfaceAllowed(HitResult.ImpactNormal), Placement, bLastTraceWasValid);

	const FVector EndPoint = HitResult.bBlockingHit ? HitResult.ImpactPoint : ViewEnd;
	if (AGameplayAbilityWorldReticle* Reticle = ReticleActor.Get())
	{
		Reticle->SetIsTargetValid(bLastTraceWasValid);
		const FRotator SurfaceRot = HitResult.bBlockingHit ? FRotationMatrix::MakeFromZ(HitResult.ImpactNormal).Rotator() : FRotator::ZeroRotator;
		Reticle->SetActorLocationAndRotation(EndPoint, SurfaceRot);
	}

#if ENABLE_DRAW_DEBUG
	if (bDebug && !bShowingPreview)
	{
		const FColor Color = bLastTraceWasValid ? FColor::Green : FColor::Red;
		DrawDebugSphere(GetWorld(), EndPoint, 20.f, 10, Color, false);
		if (HitResult.bBlockingHit)
		{
			DrawDebugLine(GetWorld(), EndPoint, EndPoint + HitResult.ImpactNormal * 60.f, Color, false, -1.f, 0, 2.f);
		}
	}
#endif

	return HitResult;
}

bool AZL_GATA_SurfaceTrace::ShouldCancelOnInvalidConfirm() const
{
	if (bCancelOnInvalidConfirm)
	{
		return true;
	}
	// Instant confirms once and Quick confirms on release: neither gets another try
	const UBase_GA_TargetActors* TargetAbility = Cast<UBase_GA_TargetActors>(OwningAbility);
	return TargetAbility && TargetAbility->TargetConfirmationStyle != EGASTargetConfirmationStyle::Confirm;
}

void AZL_GATA_SurfaceTrace::ConfirmTargeting()
{
	// Instant confirms before the first Tick, so refresh the hit here
	if (SourceActor)
	{
		PerformTrace(SourceActor);
	}

	UE_LOG(LogTemp, Log, TEXT("SurfaceTrace confirm: %s"), bLastTraceWasValid ? TEXT("valid") : TEXT("invalid"));
	if (bLastTraceWasValid)
	{
		Super::ConfirmTargeting();
		return;
	}

	if (ShouldCancelOnInvalidConfirm())
	{
		CancelTargeting();
		return;
	}

	// LocalInputConfirm clears its callbacks before broadcasting: bind again so the next press is heard
	if (GenericDelegateBoundASC)
	{
		GenericDelegateBoundASC->GenericLocalConfirmCallbacks.AddUniqueDynamic(this, &AZL_GATA_SurfaceTrace::ConfirmTargeting);
	}
}
