// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SliderBaseRow.h"

#include "CommonTextBlock.h"
#include "MVVMSubsystem.h"
#include "Components/Slider.h"
#include "UI/Settings/ScalarSettingHandler.h"
#include "UI/Settings/MVVM/ScalarSettingViewModel.h"
#include "View/MVVMView.h"

void USliderBaseRow::SetupComponents(USettingBaseViewModel* InViewModel)
{
	SliderVM =  Cast<UScalarSettingViewModel>(InViewModel);
	if (!SliderVM)
	{
		return;
	}
	if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
	{
		if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
		{
			if (bool bSuccess = WidgetView->SetViewModel(FName("ScalarSettingViewModel"), SliderVM))
			{
				NameOfRow->SetText(SliderVM->DisplayName);
				SettingsHandler = Cast<UScalarSettingHandler>(SliderVM->BaseHandler);
				if (SettingsHandler)
				{
					SliderVM->SetMaxValue(SettingsHandler->SliderMax);
					Slider->SetMaxValue(SettingsHandler->SliderMax);
					Slider->SetMinValue(SettingsHandler->SliderMin);
					SliderVM->SetMinValue(SettingsHandler->SliderMin);
					Slider->SetStepSize(SettingsHandler->SliderStep);
					SliderVM->SetStepValue(SettingsHandler->SliderStep);
					Slider->SetValue(SettingsHandler->GetCurrentValue());
				}
			}
		}
	}
}

void USliderBaseRow::OnLeftClicked()
{
	float newValue = FMath::Clamp(Slider->GetValue() - Slider->GetStepSize(), Slider->GetMinValue(), Slider->GetMaxValue());
	Slider->SetValue(newValue);
}

void USliderBaseRow::OnRightClicked()
{
	float newValue = FMath::Clamp(Slider->GetValue() + Slider->GetStepSize(), Slider->GetMinValue(), Slider->GetMaxValue());
	Slider->SetValue(newValue);
}
