//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ZL_HUD_CrosshairBase.generated.h"

class AZeroLockCharacter;

/**
 * Gun crosshair. Draws itself in NativePaint, centred on the widget's geometry: four lines around a gap
 * plus a centre dot, each with a dark outline so it reads on bright and dark backgrounds.
 * Designer content (WBP_CrosshairBase) is drawn underneath; keep it collapsed unless bDrawCrosshair is off.
 */
UCLASS()
class ZEROLOCK_API UZL_HUD_CrosshairBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairColor(FLinearColor NewColor) { Color = NewColor; }

	/** Off: draw nothing (the designer image, if any, is the crosshair). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
	bool bDrawCrosshair = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
	FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
	FLinearColor OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.65f);

	/** Outline width around every element, 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "0"))
	float OutlineThickness = 1.f;

	/** Distance from the centre to the inner end of each line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "0"))
	float Gap = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "0"))
	float LineLength = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "1"))
	float LineThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
	bool bShowTopLine = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
	bool bShowCenterDot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "1"))
	float CenterDotSize = 2.f;

protected:
	UPROPERTY()
	AZeroLockCharacter* Hero;

	UFUNCTION()
	virtual void AddToDelegate();

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
