// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Styling/SlateTypes.h"
#include "ZL_HUD_StaminaBar.generated.h"

class UCommonTextBlock;
class UHorizontalBox;
class UProgressBar;

/**
 * Deadlock-style stamina pips: one bar per stamina point, full ones lit, the regenerating one filling up.
 * Reads Stamina / MaxStamina from the owning hero's UZL_VM_Attributes (predicted on the owning client).
 */
UCLASS()
class ZEROLOCK_API UZL_HUD_StaminaBar : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Pips are created in here at runtime, one per MaxStamina point. */
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PipBox;

	/** Optional "2/3" label. */
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StaminaText;

	UPROPERTY(EditAnywhere, Category = "Stamina")
	FProgressBarStyle PipStyle;

	UPROPERTY(EditAnywhere, Category = "Stamina")
	FLinearColor FullColor = FLinearColor(1.0f, 0.85f, 0.25f);

	UPROPERTY(EditAnywhere, Category = "Stamina")
	FLinearColor RechargingColor = FLinearColor(0.45f, 0.45f, 0.45f);

	UPROPERTY(EditAnywhere, Category = "Stamina")
	FVector2D PipSize = FVector2D(48.0f, 8.0f);

	UPROPERTY(EditAnywhere, Category = "Stamina")
	float PipSpacing = 4.0f;

private:
	void Refresh(float Stamina, float MaxStamina);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> Pips;

	float LastStamina = -1.0f;
	float LastMaxStamina = -1.0f;
};
