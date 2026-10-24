// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_BaseSurfaceTargetAbility.h"

#include "AbilitySystemBlueprintLibrary.h"

UZL_BaseSurfaceTargetAbility::UZL_BaseSurfaceTargetAbility()
{
	// The location comes from the client's target data.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	Targetclass = AZL_GATA_SurfaceTrace::StaticClass();
	TargetConfirmationStyle = EGASTargetConfirmationStyle::Confirm;
	bCancelOnRepress = true;
}

void UZL_BaseSurfaceTargetAbility::OnTargetActorSpawned(AGameplayAbilityTargetActor_Trace* TargetActor)
{
	if (AZL_GATA_SurfaceTrace* SurfaceTrace = Cast<AZL_GATA_SurfaceTrace>(TargetActor))
	{
		SurfaceTrace->SurfaceMode = SurfaceMode;
		SurfaceTrace->MaxRange = MaxPlacementRange.GetValueAtLevel(GetAbilityLevel());
		SurfaceTrace->bDebug = bDrawTargetingDebug;
		SurfaceTrace->PreviewBox = GetPreviewBox();
		if (bShowPreview)
		{
			if (PreviewMesh)
			{
				SurfaceTrace->PreviewMesh = PreviewMesh;
			}
			if (PreviewMaterial)
			{
				SurfaceTrace->PreviewMaterial = PreviewMaterial;
			}
			SurfaceTrace->bFitPreviewMeshToBox = bFitPreviewMeshToBox;
			SurfaceTrace->ValidPreviewColor = ValidPreviewColor;
			SurfaceTrace->InvalidPreviewColor = InvalidPreviewColor;
		}
		SurfaceTrace->PlacementCheck = [WeakThis = TWeakObjectPtr<UZL_BaseSurfaceTargetAbility>(this)](const FHitResult& Hit, FTransform& OutPlacement)
		{
			return WeakThis.IsValid() && WeakThis->GetPlacementTransform(Hit, OutPlacement);
		};
	}
}

bool UZL_BaseSurfaceTargetAbility::GetPlacementTransform(const FHitResult& Hit, FTransform& OutTransform) const
{
	OutTransform = FTransform(FRotationMatrix::MakeFromX(Hit.ImpactNormal).ToQuat(), Hit.ImpactPoint);
	return true;
}

FBox UZL_BaseSurfaceTargetAbility::GetPreviewBox() const
{
	if (!bShowPreview)
	{
		return FBox(ForceInit);
	}
	const FVector Half(0.0f, PreviewSize.Y * 0.5f, PreviewSize.Z * 0.5f);
	return FBox(FVector(0.0f, -Half.Y, -Half.Z), FVector(PreviewSize.X, Half.Y, Half.Z));
}

bool UZL_BaseSurfaceTargetAbility::GetHitFromData(const FGameplayAbilityTargetDataHandle& Data, FHitResult& OutHit)
{
	if (!UAbilitySystemBlueprintLibrary::TargetDataHasHitResult(Data, 0))
	{
		return false;
	}
	OutHit = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Data, 0);
	return OutHit.bBlockingHit;
}

bool UZL_BaseSurfaceTargetAbility::CanConfirmTarget(const FGameplayAbilityTargetDataHandle& Data)
{
	const AActor* Hero = GetAvatarActorFromActorInfo();
	FHitResult Hit;
	if (!Hero || !GetHitFromData(Data, Hit))
	{
		return false;
	}

	// Angle rules come from the target actor class (MaxGroundAngle / WallAngleTolerance may be changed in its BP).
	const AZL_GATA_SurfaceTrace* TraceDefaults = Targetclass ? Cast<AZL_GATA_SurfaceTrace>(Targetclass->GetDefaultObject()) : nullptr;
	if (TraceDefaults && !AZL_GATA_SurfaceTrace::IsSurfaceAllowedFor(SurfaceMode, TraceDefaults->MaxGroundAngle, TraceDefaults->WallAngleTolerance, Hit.ImpactNormal))
	{
		return false;
	}

	const float Range = MaxPlacementRange.GetValueAtLevel(GetAbilityLevel()) + ServerRangeSlack;
	FTransform Placement;
	const bool bValid = FVector::DistSquared(Hero->GetActorLocation(), Hit.ImpactPoint) <= FMath::Square(Range) && GetPlacementTransform(Hit, Placement);
	if (!bValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] %s rejected location %s"), *GetName(), HasAuthority(&CurrentActivationInfo) ? TEXT("server") : TEXT("client"), *Hit.ImpactPoint.ToString());
	}
	return bValid;
}

void UZL_BaseSurfaceTargetAbility::AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data)
{
	FHitResult Hit;
	FTransform Placement;
	GetHitFromData(Data, Hit);
	GetPlacementTransform(Hit, Placement);
	PlacementConfirmed(Hit, Placement, Data);
}

void UZL_BaseSurfaceTargetAbility::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
