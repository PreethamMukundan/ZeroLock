// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "SettingsManagerViewModel.generated.h"

class USubMenuManagerViewModel;
class USettingBaseViewModel;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API USettingsManagerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, FieldNotify)
	TArray<USettingBaseViewModel*> VideoSettings;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	TArray<USettingBaseViewModel*> AudioSettings;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify)
	TMap<FName, USubMenuManagerViewModel*> SubMenuRegisterty;

	UFUNCTION(BlueprintCallable)
	void BuildSettingsFromDataTable(UDataTable* MasterTable);
	
	UFUNCTION(BlueprintCallable)
	void SubMenuRegister(FName SubName, UDataTable* SubTable);
};
