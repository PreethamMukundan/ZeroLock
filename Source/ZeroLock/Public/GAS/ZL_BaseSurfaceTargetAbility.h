// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/Base_GA_TargetActors.h"
#include "GAS/TargetActors/ZL_GATA_SurfaceTrace.h"
#include "ZL_BaseSurfaceTargetAbility.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * Base for abilities that pick one location on a surface (AZL_GATA_SurfaceTrace), then do their thing once.
 * TargetConfirmationStyle: Instant = where you aim on press, Confirm = left click (Cancel or re-press backs out for
 * free, an invalid spot keeps aiming), Quick = on release of the ability key. Instant/Quick on an invalid spot = free cancel.
 * GetPlacementTransform turns the hit into the real placement and decides whether it fits; it runs every aim tick
 * (green/red preview of GetPreviewBox, if any), on the client's confirm and on the server's re-check, so all three agree.
 * CanConfirmTarget (client and server, before the commit) also re-checks the surface angle and the range (+ ServerRangeSlack).
 * Subclasses override PlacementConfirmed (default: end the ability).
 */
UCLASS(Abstract)
class ZEROLOCK_API UZL_BaseSurfaceTargetAbility : public UBase_GA_TargetActors
{
	GENERATED_BODY()

public:
	UZL_BaseSurfaceTargetAbility();

	virtual void OnTargetActorSpawned(AGameplayAbilityTargetActor_Trace* TargetActor) override;
	virtual bool CanConfirmTarget(const FGameplayAbilityTargetDataHandle& Data) override;
	virtual void AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	EZL_SurfaceTargetMode SurfaceMode = EZL_SurfaceTargetMode::Any;

	/** Max distance from the hero to the aimed point. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	FScalableFloat MaxPlacementRange = 2000.0f;

	/** Server re-check tolerance: the hero moved a bit since the client confirmed. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float ServerRangeSlack = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	bool bDrawTargetingDebug = false;

	/** Show PreviewMesh where the thing goes while aiming, tinted ValidPreviewColor / InvalidPreviewColor. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Preview")
	bool bShowPreview = false;

	/** Preview size in cm, placement space: X out of the surface (starts on it), Y and Z centered on the aim point. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Preview", meta = (EditCondition = "bShowPreview"))
	FVector PreviewSize = FVector(100.0f, 100.0f, 100.0f);

	/** Empty = the target actor's default (engine Cube). Model it with X out of the surface. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Preview", meta = (EditCondition = "bShowPreview"))
	TObjectPtr<UStaticMesh> PreviewMesh;

	/** Stretch the mesh's bounds onto the preview box. Off: the mesh keeps its own size, pivot on the aim point. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Preview", meta = (EditCondition = "bShowPreview"))
	bool bFitPreviewMeshToBox = true;

	/** Needs a "Color" vector parameter. Empty = the target actor's default (engine BasicShapeMaterial). */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Preview", meta = (EditCondition = "bShowPreview"))
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Preview", meta = (EditCondition = "bShowPreview"))
	FLinearColor ValidPreviewColor = FLinearColor(0.1f, 0.9f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Preview", meta = (EditCondition = "bShowPreview"))
	FLinearColor InvalidPreviewColor = FLinearColor(0.9f, 0.1f, 0.1f);

	/**
	 * Hit → placement (starts as hit point, X = surface normal). False = can't place here. OutTransform should still be
	 * set when a transform exists, for the preview.
	 */
	virtual bool GetPlacementTransform(const FHitResult& Hit, FTransform& OutTransform) const;

	/** Placement-space box the preview mesh fills. Default: PreviewSize, or none while bShowPreview is off (invalid = no preview). */
	virtual FBox GetPreviewBox() const;

	/** The location passed the checks and the ability is committed (client and server). Default ends the ability. */
	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data);

	static bool GetHitFromData(const FGameplayAbilityTargetDataHandle& Data, FHitResult& OutHit);
};
