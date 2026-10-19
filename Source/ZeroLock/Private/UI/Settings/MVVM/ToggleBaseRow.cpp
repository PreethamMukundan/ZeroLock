// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/ToggleBaseRow.h"

#include "CommonTextBlock.h"
#include "MVVMSubsystem.h"
#include "Components/CheckBox.h"
#include "UI/Settings/BooleanSettingHandler.h"
#include "UI/Settings/MVVM/BooleanSettingViewModel.h"
#include "View/MVVMView.h"

void UToggleBaseRow::SetupComponents(USettingBaseViewModel* InViewModel)
{
	BoolVM =  Cast<UBooleanSettingViewModel>(InViewModel);
	if (!BoolVM)
	{
		return;
	}
	if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
	{
		if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
		{
			if (bool bSuccess = WidgetView->SetViewModel(FName("BooleanSettingViewModel"), BoolVM))
			{
				NameOfRow->SetText(BoolVM->DisplayName);
				SettingsHandler = Cast<UBooleanSettingHandler>(BoolVM->BaseHandler);
				if (SettingsHandler)
				{
					CheckBox->SetIsChecked(BoolVM->GetbIsToggled());
				}
			}
		}
	}
}

void UToggleBaseRow::NativeOnClicked()
{
	CheckBox->SetIsChecked(!(CheckBox->IsChecked()));
}
