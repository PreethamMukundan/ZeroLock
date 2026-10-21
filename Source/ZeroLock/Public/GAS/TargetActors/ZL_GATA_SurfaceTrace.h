// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "GAS/TargetActors/ZL_PlacementClearance.h"
#include "ZL_GATA_SurfaceTrace.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Which surfaces a surface trace accepts, by the angle of the hit normal. */
UENUM(BlueprintType)
enum class EZL_SurfaceTargetMode : uint8
{
	Ground	UMETA(ToolTip = "Floors: normal within MaxGroundAngle of straight up"),
	Wall	UMETA(ToolTip = "Vertical surfaces: normal within WallAngleTolerance of horizontal"),
	Any		UMETA(ToolTip = "Any angle, ceilings included")
};

/**
 * Camera trace against WorldStatic only (pawns never block the aim). The hit is valid when its normal matches
 * SurfaceMode and it is within MaxRange of the hero. Produces a SingleTargetHit (location + ImpactNormal).
 * Invalid confirm: Confirm style keeps aiming; Instant/Quick (or bCancelOnInvalidConfirm) cancel.
 * Optional placement: the owning ability's PlacementCheck turns the hit into where the thing goes and adds its own
 * checks (e.g. Doorway: the door fits); with a valid PreviewBox the aiming player sees PreviewMesh there, green/red.
 */
UCLASS(Blueprintable)
class ZEROLOCK_API AZL_GATA_SurfaceTrace : public AGameplayAbilityTargetActor_Trace
{
	GENERATED_BODY()
public:
	AZL_GATA_SurfaceTrace();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Surface)
	EZL_SurfaceTargetMode SurfaceMode = EZL_SurfaceTargetMode::Any;

	/** Ground mode: max angle between the normal and up, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true, ClampMin = 0, ClampMax = 90), Category = Surface)
	float MaxGroundAngle = 45.f;

	/** Wall mode: max angle between the normal and the horizontal plane, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true, ClampMin = 0, ClampMax = 90), Category = Surface)
	float WallAngleTolerance = 20.f;

	/** If > 0, hits further than this from AnchorLocation are invalid (e.g. Doorway's second door). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true, ClampMin = 0), Category = Surface)
	float MaxAnchorDistance = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Surface)
	FVector AnchorLocation = FVector::ZeroVector;

	/** Hits on these actors are invalid (e.g. an existing door). */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> InvalidActors;

	/** Cancel on an invalid confirm even when the owning ability uses the Confirm style. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Surface)
	bool bCancelOnInvalidConfirm = false;

	/**
	 * Set by the owning ability (C++): placement transform for a hit (OutPlacement starts as the hit point, X = normal)
	 * and whether it's allowed. Runs every aim tick, so keep it cheap.
	 */
	TFunction<bool(const FHitResult& Hit, FTransform& OutPlacement)> PlacementCheck;

	/**
	 * Optional no-clip rules, tested on the placement against PreviewBox (needs a valid PreviewBox). Off by default:
	 * placements may clip. Abilities that check in their own PlacementCheck (Doorway) leave this off.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = "Surface|Preview")
	FZL_PlacementClearance Clearance;

	/** Preview size in placement space (cm); invalid box = no preview. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Surface|Preview")
	FBox PreviewBox = FBox(ForceInit);

	/** Scaled to PreviewBox (assumed a 100 cm cube centered on its origin, like the engine Cube). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Surface|Preview")
	TObjectPtr<UStaticMesh> PreviewMesh;

	/** Needs a "Color" vector parameter (engine BasicShapeMaterial by default). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Surface|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Surface|Preview")
	FLinearColor ValidPreviewColor = FLinearColor(0.1f, 0.9f, 0.2f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Surface|Preview")
	FLinearColor InvalidPreviewColor = FLinearColor(0.9f, 0.1f, 0.1f);

	UFUNCTION(BlueprintPure, Category = Surface)
	bool IsCurrentTargetValid() const { return bLastTraceWasValid; }

	UFUNCTION(BlueprintPure, Category = Surface)
	bool IsSurfaceAllowed(const FVector& Normal) const;

	virtual void ConfirmTargeting() override;

protected:
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;

	bool ShouldCancelOnInvalidConfirm() const;

	/** Shows/moves/colors the preview. Only for the player aiming on this machine. Returns whether it is shown. */
	bool UpdatePreview(bool bHasHit, const FTransform& Placement, bool bValid);

	UPROPERTY(VisibleAnywhere, Category = "Surface|Preview")
	TObjectPtr<UStaticMeshComponent> PreviewComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID;

	bool bLastTraceWasValid = false;
	bool bPreviewShowsValid = false;
};
