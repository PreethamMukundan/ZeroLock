// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/RotatorBaseRow.h"

#include "CommonTextBlock.h"
#include "CommonRotator.h"
#include "MVVMSubsystem.h"
#include "UI/Settings/SelectorSettingHandler.h"
#include "UI/Settings/MVVM/SelectorSettingsViewModel.h"
#include "View/MVVMView.h"
#include "ZeroLock/ZeroLock.h"

void URotatorBaseRow::SetupComponents(USettingBaseViewModel* InViewModel)
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
					Rotator->PopulateTextLabels(SettingsHandler->GetSelectedNames());
					Rotator->SetSelectedItem(0);
				}
			}
		}
	}
}

void URotatorBaseRow::OnLeftClicked()
{
	Rotator->ShiftTextLeft();
}

void URotatorBaseRow::OnRightClicked()
{
	Rotator->ShiftTextRight();
}

void URotatorBaseRow::ApplyOnSelectionChanged(FString NewValue)
{
	SettingsHandler->OnApplySelectionString(FName(NewValue));
}

void URotatorBaseRow::IntApplyOnSelectionChanged(int32 NewValue)
{
	SettingsHandler->OnApplySelection(NewValue);
}
