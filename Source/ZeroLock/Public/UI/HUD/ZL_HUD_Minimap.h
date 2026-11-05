// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Styling/SlateBrush.h"
#include "ZL_HUD_Minimap.generated.h"

class AZL_MinimapVolume;
class AZero_BasePlayerState;
class UTexture2D;

/** A hero's portrait on the map. A UPROPERTY so the brush keeps the icon texture alive. */
USTRUCT()
struct FZL_MinimapHeroIcon
{
	GENERATED_BODY()

	UPROPERTY()
	FSlateBrush Brush;

	/** Disc behind the icon. */
	UPROPERTY()
	FLinearColor Fill = FLinearColor::Black;

	UPROPERTY()
	bool bHasIcon = false;
};

/**
 * Whole-map HUD minimap, fixed orientation (own base at the bottom). Draws itself in NativePaint inside the widget's
 * geometry, letterboxed to the map's shape: background panel, map (AZL_MinimapVolume), coloured lanes, base markers,
 * then one round hero portrait per player with a team ring and a view cone for the local player.
 * Enemies only show while an ally has line of sight, like fog of war. Hidden without a minimap volume in the level.
 * The size comes from the widget BP (WBP_Minimap, a SizeBox root).
 */
UCLASS()
class ZEROLOCK_API UZL_HUD_Minimap : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UZL_HUD_Minimap(const FObjectInitializer& ObjectInitializer);

	/** Off: every enemy is always on the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Vision")
	bool bHideUnseenEnemies = true;

	/** How far an ally can reveal an enemy, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Vision", meta = (ClampMin = "0"))
	float VisionRange = 6000.f;

	/** Seconds between line of sight checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Vision", meta = (ClampMin = "0.02"))
	float VisionInterval = 0.2f;

	/** Seconds an enemy stays on the map after it was last seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Vision", meta = (ClampMin = "0"))
	float RevealLinger = 1.f;

	/** Rotate the map 180 degrees when needed so the local team's base is at the bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout")
	bool bKeepOwnBaseAtBottom = true;

	/** Gap between the widget's edge and the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "0"))
	float MapPadding = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "0"))
	float PanelCornerRadius = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "1"))
	float HeroIconSize = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "1"))
	float LocalHeroIconSize = 32.f;

	/** Team ring width around each portrait. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "0"))
	float RingThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "0"))
	float LaneThickness = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "1"))
	float BaseMarkerSize = 16.f;

	/** Full width of the view cone, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "1", ClampMax = "180"))
	float ViewConeAngle = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Layout", meta = (ClampMin = "0"))
	float ViewConeLength = 58.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
	FLinearColor PanelColor = FLinearColor(0.f, 0.f, 0.f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
	FLinearColor AllyColor = FLinearColor(0.20f, 0.62f, 1.00f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
	FLinearColor EnemyColor = FLinearColor(0.95f, 0.22f, 0.20f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
	FLinearColor LocalColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
	FLinearColor ViewConeColor = FLinearColor(1.f, 1.f, 1.f, 0.4f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors", meta = (ClampMin = "0", ClampMax = "1"))
	float LaneOpacity = 0.85f;

	/** Portrait for heroes that aren't in DT_HeroList. Without one they get a plain disc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Colors")
	TObjectPtr<UTexture2D> DefaultHeroIcon;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FMarker
	{
		FVector2f MapPosition = FVector2f::ZeroVector;
		UClass* HeroClass = nullptr;
		float Yaw = 0.f;
		bool bAlly = true;
		bool bLocal = false;
	};

	void RefreshMarkers(float DeltaTime);
	void RefreshVision(const TArray<APawn*>& Allies, const TArray<APawn*>& Enemies);
	bool ShouldFlipMap(const AZero_BasePlayerState* LocalPlayer) const;
	const FZL_MinimapHeroIcon& GetHeroIcon(UClass* HeroClass);

	TWeakObjectPtr<AZL_MinimapVolume> Volume;
	float NextVolumeSearch = 0.f;

	TArray<FMarker> Markers;
	bool bFlipMap = false;

	/** World time each enemy pawn was last seen. */
	TMap<TWeakObjectPtr<APawn>, double> LastSeenTime;
	float VisionCountdown = 0.f;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UClass>, FZL_MinimapHeroIcon> HeroIcons;

	UPROPERTY(Transient)
	FSlateBrush MapBrush;

	/** Plain white circle and panel; tinted when drawn. */
	FSlateBrush CircleBrush;
	FSlateBrush PanelBrush;
};
