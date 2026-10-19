// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SettingsManagerViewModel.h"

#include "UI/Settings/SettingHandlerBase.h"
#include "UI/Settings/MVVM/SubMenuManagerViewModel.h"

void USettingsManagerViewModel::BuildSettingsFromDataTable(UDataTable* MasterTable)
{
	if (!MasterTable)
	{
		UE_LOG(LogTemp, Error, TEXT("MasterTable is null!"));
		return;
	}
	static const FString ContextString(TEXT("MasterSettingsContext"));
    

	TArray<FMasterSubSetting*> AllRows;
	MasterTable->GetAllRows<FMasterSubSetting>(ContextString, AllRows);
	
	for (FMasterSubSetting* RowData : AllRows)
	{
		if (!RowData) continue; 
		
		UDataTable* LoadedSubTable = RowData->SubTable.LoadSynchronous();
		
		if (LoadedSubTable)
		{
			SubMenuRegister(RowData->SettingCategory, LoadedSubTable);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load SubTable for category: %s"), *RowData->SettingCategory.ToString());
		}
	}
}

void USettingsManagerViewModel::SubMenuRegister(FName SubName, UDataTable* SubTable)
{
	if (!SubTable)
	{
		UE_LOG(LogTemp, Error, TEXT("SubTable is null!"));
		return;
	}
	if (SubMenuRegisterty.Contains(SubName))
	{
		UE_LOG(LogTemp, Error, TEXT("SubTable is Name Duplicate"));
		return;
	}
	USubMenuManagerViewModel* NewSubMenu = NewObject<USubMenuManagerViewModel>(this);
	if (!NewSubMenu)
	{
		UE_LOG(LogTemp, Error, TEXT("NewSubMenu is null!"));
		return;
	}
	NewSubMenu->BuildSettingsFromDataTable(SubTable);
	SubMenuRegisterty.Add(SubName, NewSubMenu);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SubMenuRegisterty);
}
