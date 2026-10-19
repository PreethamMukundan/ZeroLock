// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "SubMenuManagerViewModel.generated.h"

class USettingBaseViewModel;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API USubMenuManagerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, FieldNotify)
	TArray<USettingBaseViewModel*> SettingsArray;
	
	UFUNCTION(BlueprintCallable)
	void BuildSettingsFromDataTable(UDataTable* MasterTable);
};
