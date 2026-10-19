// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "SettingBaseViewModel.h"
#include "SelectorSettingsViewModel.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API USelectorSettingsViewModel : public USettingBaseViewModel
{
	GENERATED_BODY()
	
	public:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	TArray<FString> SelectionOptions;
	
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	FName SelectionOptionName;
	
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	int32 SelectionOptionIndex;
	
	
	void SetSelectionOptionName(const FName NewValue);
	void SetSelectionOptionIndex(const int32 NewValue);
	
	FName GetSelectionOptionName() const{ return SelectionOptionName; };
	int32 GetSelectionOptionIndex() const{ return SelectionOptionIndex; };
	
	void SetSelectionOptions(const TArray<FString> SelectionOptions);
	TArray<FString> GetSelectionOptions()const { return SelectionOptions; };
	
	
	UFUNCTION(BlueprintCallable)
	void OnSelectionChanged(int32 NewValue);
	
	UFUNCTION(BlueprintCallable)
	void OnValueChangedString(FName NewValue);
	
	
	UFUNCTION(BlueprintCallable)
	void StringOnValueChanged(FString NewValue);
	
	
};
