// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZeroLock/ZeroLock.h"
#include "ZL_MinimapVolume.generated.h"

class UBoxComponent;
class UTexture2D;

/** A lane on the map: one zipline, in map space (0-1, see AZL_MinimapVolume::WorldToMap). */
struct FZL_MinimapLane
{
	TArray<FVector2f> Points;
	FLinearColor Color = FLinearColor::White;
};

/** A team's base: the average of its player starts, in map space. */
struct FZL_MinimapBase
{
	FVector2f Position = FVector2f::ZeroVector;
	ETeamID Team = ETeamID::TeamNull;
};

/**
 * Place one per map: the box is the area the minimap shows (keep it axis aligned; top must clear the tallest geometry).
 * Map space: +X is up, +Y is right, so a map with its bases on the X axis reads as a tall map.
 * Unless MapTextureOverride is set, it draws the map itself after the level has streamed in: a grid of downward traces
 * against WorldStatic, time-sliced so it never hitches, coloured by height with outlines. The result is a transient
 * texture kept here, so a HUD rebuild (hero swap) doesn't redo it, and it can't go stale when the level is rebuilt.
 * Doesn't run on dedicated servers. Lanes come from the AZero_ZiplineActors, bases from the AZero_BasePlayerStarts.
 */
UCLASS(Blueprintable)
class ZEROLOCK_API AZL_MinimapVolume : public AActor
{
	GENERATED_BODY()

public:
	AZL_MinimapVolume();

	virtual void Tick(float DeltaSeconds) override;

	/** The world's minimap volume, if it has one. */
	static AZL_MinimapVolume* Find(const UWorld* World);

	/** Null until the bake finishes. */
	UTexture2D* GetMapTexture() const;

	/** World to map space: (0,0) top left, (1,1) bottom right. */
	FVector2f WorldToMap(const FVector& WorldLocation) const;

	/** Inside the box, Z included, so pocket spaces outside it (Doorman's hotel) stay off the map. */
	bool ContainsWorld(const FVector& WorldLocation) const;

	/** Map width / height. */
	float GetMapAspect() const;

	const TArray<FZL_MinimapLane>& GetLanes() const { return Lanes; }
	const TArray<FZL_MinimapBase>& GetBases() const { return Bases; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<UBoxComponent> MapBounds;

	/** Hand-made map art for this level. Must match the box: top = +X, right = +Y. Skips the bake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<UTexture2D> MapTextureOverride;

	/** Pixels along the longer side of the box. Cost is about Resolution^2 traces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Bake", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 Resolution = 512;

	/** Tracing time per frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Bake", meta = (ClampMin = "0.5"))
	float BakeBudgetMs = 4.f;

	/** World Z of the main walkable floor. Heights are coloured relative to this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Bake")
	float GroundZ = 0.f;

	/** Up to this far above GroundZ still counts as street (curbs, low crates). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Bake", meta = (ClampMin = "0"))
	float StreetMaxHeight = 120.f;

	/** Further than this below GroundZ counts as a pit (drop holes, tunnel mouths). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Bake", meta = (ClampMin = "0"))
	float PitDepth = 100.f;

	/** Blocks reach BlockHighColor at this height above GroundZ. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Bake", meta = (ClampMin = "1"))
	float BlockMaxHeight = 1600.f;

	/** A height step of at least this between neighbouring pixels gets an outline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Bake", meta = (ClampMin = "1"))
	float EdgeHeight = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Colors")
	FLinearColor StreetColor = FLinearColor(0.135f, 0.155f, 0.175f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Colors")
	FLinearColor BlockLowColor = FLinearColor(0.040f, 0.048f, 0.058f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Colors")
	FLinearColor BlockHighColor = FLinearColor(0.012f, 0.015f, 0.020f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Colors")
	FLinearColor PitColor = FLinearColor(0.004f, 0.005f, 0.007f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Colors")
	FLinearColor EdgeColor = FLinearColor(0.34f, 0.39f, 0.44f, 1.f);

	/** Handed out by mean Y, so with +Y right: rightmost lane first. Repeats if there are more ziplines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Colors")
	TArray<FLinearColor> LaneColors;

	/** Spacing of the points sampled along each zipline, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Lanes", meta = (ClampMin = "50"))
	float LaneSampleStep = 400.f;

protected:
	virtual void BeginPlay() override;

private:
	bool AreLevelsReady() const;
	void CollectFeatures();
	void BakeRows();
	void FinishBake();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BakedTexture;

	TArray<FZL_MinimapLane> Lanes;
	TArray<FZL_MinimapBase> Bases;

	/** Surface height per pixel, row 0 = max X. NoHit where the trace found nothing. */
	TArray<float> Heights;
	int32 BakeWidth = 0;
	int32 BakeHeight = 0;
	int32 NextPixel = 0;
	bool bCollected = false;
};
