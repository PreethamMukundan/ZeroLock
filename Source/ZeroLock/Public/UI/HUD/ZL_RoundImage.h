// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "ZL_RoundImage.generated.h"

/**
 * An Image that always draws as a circle. Only the paint changes: Brush, ColorAndOpacity and anything bound to them
 * (MVVM included) work exactly as on a plain Image, so a binding that replaces the whole brush still comes out round.
 * Size it with its parent (a SizeBox), since bound brushes bring their own ImageSize.
 * RadialProgress draws a clockwise cooldown sweep on top; bind it in View Bindings like any other property.
 */
UCLASS()
class ZEROLOCK_API UZL_RoundImage : public UImage
{
	GENERATED_BODY()

public:
	/** Ring drawn inside the edge of the circle, 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round Image", meta = (ClampMin = "0"))
	float OutlineThickness = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round Image")
	FLinearColor OutlineColor = FLinearColor(0.75f, 0.75f, 0.75f, 0.9f);

	/** 0 to 1, clockwise from the top: the part not yet reached is covered by the sweep. 0 and 1 draw nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = SetRadialProgress, Category = "Round Image", meta = (ClampMin = "0", ClampMax = "1"))
	float RadialProgress = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round Image")
	FLinearColor RadialColor = FLinearColor(0.f, 0.f, 0.f, 0.65f);

	/** Extends the sweep past the image's edge, to cover a larger circle behind an inset image. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round Image", meta = (ClampMin = "0"))
	float RadialOverscan = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Round Image")
	void SetRadialProgress(float InProgress);

	float GetRadialProgress() const { return RadialProgress; }

	virtual void SynchronizeProperties() override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
