// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/MVVM/SettingBaseViewModel.h"
#include "ScalarSettingViewModel.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UScalarSettingViewModel : public USettingBaseViewModel
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	float CurrentValue;
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	float MaxValue;
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	float MinValue;
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	float StepValue;

	UFUNCTION(BlueprintCallable)
	void ApplySliderValue(float NewValue);
	
	void SetCurrentValue(const float NewValue);
	float GetCurrentValue()const { return CurrentValue; };
	
	void SetMaxValue(const float NewValue);
	float GetMaxValue()const { return MaxValue; };
	
	void SetMinValue(const float NewValue);
	float GetMinValue()const { return MinValue; };
	
	void SetStepValue(const float NewValue);
	float GetStepValue()const { return StepValue; };
	
};
