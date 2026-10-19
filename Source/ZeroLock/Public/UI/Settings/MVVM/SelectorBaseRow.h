// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/MVVM/SettingsBaseRow.h"
#include "SelectorBaseRow.generated.h"

class UComboBoxString;
class USelectorSettingHandler;
class USelectorSettingsViewModel;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API USelectorBaseRow : public USettingsBaseRow
{
	GENERATED_BODY()
	
public:
	
	virtual void SetupComponents(USettingBaseViewModel* InViewModel) override;
	
	UPROPERTY(EditAnywhere)
	USelectorSettingsViewModel* SelectorVM;
	
	UPROPERTY(EditAnywhere)
	USelectorSettingHandler* SettingsHandler;
	
	virtual void OnLeftClicked() override;
	virtual void OnRightClicked() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,meta=(BindWidget))
	UComboBoxString* ComboBoxString;
	
	UFUNCTION(BlueprintCallable)
	void ApplyOnSelectionChanged(FString NewValue);
	
	UFUNCTION(BlueprintCallable)
	void IntApplyOnSelectionChanged(int32 NewValue);
};
