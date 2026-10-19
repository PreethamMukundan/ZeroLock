// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/SettingHandlerBase.h"
#include "SelectorSettingHandler.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class ZEROLOCK_API USelectorSettingHandler : public USettingHandlerBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	void OnApplySelection(int32 SelectedIndex);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	void OnApplySelectionString(FName SelectiopnText);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	int32 GetCurrentSelection();
	
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	FString GetCurrentSelectionName();
	
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	TArray<FString> GetAllSelectionNames();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slections")
	TArray<FString> AllSelectionNames;
	
	UFUNCTION(BlueprintCallable, Category = "Settings Logic")
	TArray<FText> GetSelectedNames();
};
