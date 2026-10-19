// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/ZL_SettingsListView.h"

#include "UI/Settings/MVVM/SettingBaseViewModel.h"
#include "UI/Settings/MVVM/SettingsBaseRow.h"

TSubclassOf<UUserWidget> UZL_SettingsListView::GetDesiredEntryClassForItem(UObject* Item) const
{
	if (USettingBaseViewModel* VM = Cast<USettingBaseViewModel>(Item))
	{
		if (TSubclassOf<USettingsBaseRow> ClassToSpawn = SettingSubClassList.FindRef(VM->GetInteractionType()))
		{
			return ClassToSpawn;
		}
	}
	return Super::GetDesiredEntryClassForItem(Item);
}
