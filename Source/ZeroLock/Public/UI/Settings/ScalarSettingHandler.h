// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/SettingHandlerBase.h"
#include "ScalarSettingHandler.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class ZEROLOCK_API UScalarSettingHandler : public USettingHandlerBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	void OnApplySliderValue(float NewValue);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	float GetCurrentValue();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slider")
	float SliderMax =1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slider")
	float SliderMin =0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slider")
	float SliderStep =0.1;
	
};
