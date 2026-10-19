// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/MVVM/SettingsBaseRow.h"
#include "ToggleBaseRow.generated.h"

class UBooleanSettingHandler;
class UBooleanSettingViewModel;
class UCheckBox;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UToggleBaseRow : public USettingsBaseRow
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite,meta=(BindWidget))
	UCheckBox* CheckBox;
	
	virtual void SetupComponents(USettingBaseViewModel* InViewModel) override;
	
	UPROPERTY(EditAnywhere)
	UBooleanSettingViewModel* BoolVM;
	
	UPROPERTY(EditAnywhere)
	UBooleanSettingHandler* SettingsHandler;
	
	
	virtual void NativeOnClicked() override;
};
