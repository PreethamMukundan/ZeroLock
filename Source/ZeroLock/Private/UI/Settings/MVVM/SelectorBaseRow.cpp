// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SelectorBaseRow.h"

#include "CommonTextBlock.h"
#include "MVVMSubsystem.h"
#include "Components/ComboBoxString.h"
#include "UI/Settings/ScalarSettingHandler.h"
#include "UI/Settings/SelectorSettingHandler.h"
#include "UI/Settings/MVVM/SelectorSettingsViewModel.h"
#include "View/MVVMView.h"
#include "ZeroLock/ZeroLock.h"

void USelectorBaseRow::SetupComponents(USettingBaseViewModel* InViewModel)
{
	SelectorVM =  Cast<USelectorSettingsViewModel>(InViewModel);
	if (!SelectorVM)
	{
		return;
	}
	
	if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
	{
		if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
		{
			if (bool bSuccess = WidgetView->SetViewModel(FName("SelectorSettingsViewModel"), SelectorVM))
			{
				ZLOG("VM init Done");
				NameOfRow->SetText(SelectorVM->DisplayName);
				SettingsHandler = Cast<USelectorSettingHandler>(SelectorVM->BaseHandler);
				if (SettingsHandler)
				{
					for (FString Option : SettingsHandler->AllSelectionNames)
					{
						ZLOG(Option);
						ComboBoxString->AddOption(Option);
					}
					ComboBoxString->SetSelectedIndex(0);
				}
			}
		}
	}
}

void USelectorBaseRow::OnLeftClicked()
{
	int32 NewIndex = FMath::Clamp(ComboBoxString->GetSelectedIndex() -1, 0,ComboBoxString->GetOptionCount());
	ComboBoxString->SetSelectedIndex(NewIndex);
}

void USelectorBaseRow::OnRightClicked()
{
	int32 NewIndex = FMath::Clamp(ComboBoxString->GetSelectedIndex() + 1, 0,ComboBoxString->GetOptionCount());
	ComboBoxString->SetSelectedIndex(NewIndex);
}

void USelectorBaseRow::ApplyOnSelectionChanged(FString NewValue)
{
	SettingsHandler->OnApplySelectionString(FName(NewValue));
}

void USelectorBaseRow::IntApplyOnSelectionChanged(int32 NewValue)
{
	SettingsHandler->OnApplySelection(NewValue);
}
