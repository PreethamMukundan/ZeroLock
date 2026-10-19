// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SettingsManagerSubsystem.h"

#include "UI/Settings/MVVM/SettingsManagerViewModel.h"
#include "ZeroLock/ZeroLock.h"

void USettingsManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	
}

USettingsManagerViewModel* USettingsManagerSubsystem::GetSettingsVM()
{
	if (SettingsManagerVM)
	{
		return SettingsManagerVM;
	}
	else
	{
		SettingsManagerVM = NewObject<USettingsManagerViewModel>(this);
	
		UDataTable* LoadedTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/ZERO/UI/Settings/Handlers/DT_MasterSettings.DT_MasterSettings"));
	

		if (LoadedTable && SettingsManagerVM)
		{
			ZLOG("SucessDataTableLoad");
			SettingsManagerVM->BuildSettingsFromDataTable(LoadedTable);
			
		}
		return SettingsManagerVM;
	}
}
