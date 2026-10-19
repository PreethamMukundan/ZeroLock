// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/MVVM/SettingsBaseRow.h"
#include "SliderBaseRow.generated.h"

class UScalarSettingHandler;
class UScalarSettingViewModel;
class USlider;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API USliderBaseRow : public USettingsBaseRow
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite,meta=(BindWidget))
	USlider* Slider;
	
	virtual void SetupComponents(USettingBaseViewModel* InViewModel) override;
	
	UPROPERTY(EditAnywhere)
	UScalarSettingViewModel* SliderVM;
	
	UPROPERTY(EditAnywhere)
	UScalarSettingHandler* SettingsHandler;
	
	virtual void OnLeftClicked() override;
	virtual void OnRightClicked() override;
};
