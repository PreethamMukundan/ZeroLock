// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseSurfaceTargetAbility.h"
#include "HeroKit/ZL_Kit_Zone.h"
#include "ZL_Kit_GroundAbility.generated.h"

/**
 * Hero kit: pick a spot on the ground (UZL_BaseSurfaceTargetAbility: Instant, Confirm or Quick), then on the server after
 * ImpactDelay: an area hit there (ImpactRadius) and/or a zone (rain of arrows, smoke, mines, time field).
 */
UCLASS()
class ZEROLOCK_API UZL_Kit_GroundAbility : public UZL_BaseSurfaceTargetAbility
{
	GENERATED_BODY()

public:
	UZL_Kit_GroundAbility();

protected:
	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data) override;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FScalableFloat PowerScale = 1.0f;

	/** Seconds from the confirm to the impact and the zone. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float ImpactDelay = 0.0f;

	/** 0 = no impact hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float ImpactRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload ImpactEnemyPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload ImpactAllyPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitZoneSpec Zone;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	TSubclassOf<AZL_Kit_Zone> ZoneClass;

	/** On the caster at the confirm. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload SelfPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bDrawDebug = true;

	/** Cue at the impact; radius = ImpactRadius (or the zone's). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|FX")
	FZL_KitCue ImpactCue;

	/** Server, at the impact. */
	virtual void DoImpact(const FVector& Location);

	/** While aiming, a disc as wide as the hit (the bigger of ImpactRadius and the zone's radius) on the aimed spot. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bShowAreaPreview = true;

	virtual float GetPreviewDiscRadius() const override;
};
