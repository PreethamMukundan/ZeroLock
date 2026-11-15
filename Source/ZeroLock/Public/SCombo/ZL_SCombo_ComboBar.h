// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZL_SCombo_ComboBar.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * SCombo's combo meter, above the ability bar. Built in C++, added through UZL_SCombo_ComboPassive's AbilityHUDWidget, so only
 * heroes with that ability get it. Fills with the combo stacks and turns ReadyColor once the ultimate can be cast.
 */
UCLASS()
class ZEROLOCK_API UZL_SCombo_ComboBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Combo Bar")
	FVector2D BarSize = FVector2D(320.0f, 22.0f);

	/** Gap between the bar's bottom edge and the bottom of the screen. */
	UPROPERTY(EditDefaultsOnly, Category = "Combo Bar")
	float BottomOffset = 170.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combo Bar")
	FLinearColor BuildingColor = FLinearColor(1.0f, 0.55f, 0.1f);

	UPROPERTY(EditDefaultsOnly, Category = "Combo Bar")
	FLinearColor ReadyColor = FLinearColor(0.1f, 0.9f, 1.0f);

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildBar();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	int32 ShownStacks = -1;
};
