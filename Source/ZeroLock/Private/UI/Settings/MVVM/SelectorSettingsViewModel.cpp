// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SelectorSettingsViewModel.h"

#include "UI/Settings/SelectorSettingHandler.h"

void USelectorSettingsViewModel::SetSelectionOptionName(const FName NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(SelectionOptionName,NewValue))
	{
		OnValueChangedString(NewValue);
	}
}

void USelectorSettingsViewModel::SetSelectionOptionIndex(const int32 NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(SelectionOptionIndex,NewValue))
	{
		OnSelectionChanged(NewValue);
	}
}

void USelectorSettingsViewModel::SetSelectionOptions(const TArray<FString> XSelectionOptions)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(SelectionOptions,XSelectionOptions))
	{
		
	}
}

void USelectorSettingsViewModel::OnSelectionChanged(int32 NewValue)
{
	if (USelectorSettingHandler* ScalarHandler = Cast<USelectorSettingHandler>(BaseHandler))
	{
		ScalarHandler->OnApplySelection(NewValue);
	}
}

void USelectorSettingsViewModel::OnValueChangedString(FName NewValue)
{

	if (USelectorSettingHandler* ScalarHandler = Cast<USelectorSettingHandler>(BaseHandler))
	{
		ScalarHandler->OnApplySelectionString(NewValue);
	}
}

void USelectorSettingsViewModel::StringOnValueChanged(FString NewValue)
{
	if (USelectorSettingHandler* ScalarHandler = Cast<USelectorSettingHandler>(BaseHandler))
	{
		ScalarHandler->OnApplySelectionString(FName(NewValue));
	}
}
