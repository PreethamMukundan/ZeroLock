// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "ZL_GATA_ConeSingleTarget.generated.h"

class AZeroLockCharacter;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Which heroes the cone can pick. The cone never picks the caster (self-cast goes through ConfirmOnSelf). */
UENUM(BlueprintType)
enum class EZL_ConeTargetTeam : uint8
{
	Enemies,
	Allies,
	Any
};

/**
 * Picks one hero in a cone in front of the player: within MaxRange of the hero's view point, within ConeHalfAngle
 * of the hero's aim (UBaseGameplayAbility::GetHeroAimDirection), matching TargetTeam, alive, in line of sight
 * (WorldStatic), and passing the owning ability's TargetCheck. Of those, the one closest to the aim line wins.
 * The target gets IndicatorMesh at its feet (only the aiming player sees it). Produces a SingleTargetHit on the target.
 * ConfirmOnSelf (or confirming with nobody in the cone while bSelfCastWithoutTarget) produces it on the caster instead.
 * Invalid confirm: Confirm style keeps aiming; Instant/Quick cancel. Scans only on the aiming player's machine.
 */
UCLASS(Blueprintable)
class ZEROLOCK_API AZL_GATA_ConeSingleTarget : public AGameplayAbilityTargetActor_Trace
{
	GENERATED_BODY()
public:
	AZL_GATA_ConeSingleTarget();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true, ClampMin = 0, ClampMax = 90), Category = Cone)
	float ConeHalfAngle = 12.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Cone)
	EZL_ConeTargetTeam TargetTeam = EZL_ConeTargetTeam::Enemies;

	/** Confirming with nobody in the cone targets the caster instead of keeping aiming. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Cone)
	bool bSelfCastWithoutTarget = false;

	/** Set by the owning ability (C++): extra per-target rule (e.g. Hotel Guest: not already a guest). */
	TFunction<bool(const AZeroLockCharacter* Target)> TargetCheck;

	/** Placed flat at the target's feet, scaled to IndicatorRadius (assumed 100 cm wide like the engine Cylinder). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cone|Indicator")
	TObjectPtr<UStaticMesh> IndicatorMesh;

	/** Needs a "Color" vector parameter (engine BasicShapeMaterial by default). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cone|Indicator")
	TObjectPtr<UMaterialInterface> IndicatorMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cone|Indicator")
	FLinearColor IndicatorColor = FLinearColor(0.1f, 0.9f, 0.2f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cone|Indicator")
	float IndicatorRadius = 60.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cone|Indicator")
	float IndicatorThickness = 3.0f;

	UFUNCTION(BlueprintPure, Category = Cone)
	AZeroLockCharacter* GetCurrentTarget() const { return CurrentTarget.Get(); }

	virtual void ConfirmTargeting() override;

	/** Confirms with the caster as the target (self-cast). Only on the aiming player's machine. */
	void ConfirmOnSelf();

	/** The cone rules for one candidate, from the hero's view point along AimDir. Usable by the ability's server re-check. */
	static bool IsValidConeTarget(const AZeroLockCharacter* Hero, const AZeroLockCharacter* Target, const FVector& Origin, const FVector& AimDir,
		float Range, float HalfAngleDeg, EZL_ConeTargetTeam Team, float* OutAngleDeg = nullptr);

protected:
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;

	void UpdateIndicator();

	UPROPERTY(VisibleAnywhere, Category = "Cone|Indicator")
	TObjectPtr<UStaticMeshComponent> IndicatorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IndicatorMID;

	TWeakObjectPtr<AZeroLockCharacter> CurrentTarget;

	/** Set by ConfirmOnSelf for the trace that builds the target data. */
	bool bConfirmOnSelf = false;
};
