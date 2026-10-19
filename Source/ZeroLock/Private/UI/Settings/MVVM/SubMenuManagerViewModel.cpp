// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SubMenuManagerViewModel.h"

#include "UI/Settings/SettingHandlerBase.h"
#include "UI/Settings/MVVM/BooleanSettingViewModel.h"
#include "UI/Settings/MVVM/ScalarSettingViewModel.h"
#include "UI/Settings/MVVM/SelectorSettingsViewModel.h"

void USubMenuManagerViewModel::BuildSettingsFromDataTable(UDataTable* MasterTable)
{
	if (!MasterTable) return;

	static const FString ContextString(TEXT("SettingsManagerContext"));
	TArray<FMasterSettingRow*> Rows;
	MasterTable->GetAllRows<FMasterSettingRow>(ContextString, Rows);

	for (FMasterSettingRow* Row : Rows)
	{
		if (!Row || !Row->CustomHandlerClass) continue;

		USettingBaseViewModel* NewVM = nullptr;
		switch (Row->InteractionType)
		{
		case ESettingInteractionType::Slider:
			NewVM = NewObject<UScalarSettingViewModel>(this);
			break;
		case ESettingInteractionType::Toggle:
			NewVM = NewObject<UBooleanSettingViewModel>(this);
			break;
		case ESettingInteractionType::Discrete:
			NewVM = NewObject<USelectorSettingsViewModel>(this);
			break;
		default:
			break;
		}

		if (NewVM)
		{
			NewVM->SetDisplayName(Row->DisplayName);
			NewVM->SetInteractionType(Row->InteractionType);

			NewVM->BaseHandler = NewObject<USettingHandlerBase>(NewVM, Row->CustomHandlerClass);

			SettingsArray.Add(NewVM);
		}
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SettingsArray);
}
