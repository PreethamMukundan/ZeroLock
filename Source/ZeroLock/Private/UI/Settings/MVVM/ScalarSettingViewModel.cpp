// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/ScalarSettingViewModel.h"

#include "UI/Settings/ScalarSettingHandler.h"

void UScalarSettingViewModel::ApplySliderValue(float NewValue)
{
	if (UScalarSettingHandler* ScalarHandler = Cast<UScalarSettingHandler>(BaseHandler))
	{
		ScalarHandler->OnApplySliderValue(NewValue);
	}
}

void UScalarSettingViewModel::SetCurrentValue(const float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(CurrentValue,NewValue))
	{
		ApplySliderValue(NewValue);
	}
}

void UScalarSettingViewModel::SetMaxValue(const float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxValue,NewValue))
	{
		
	}
}

void UScalarSettingViewModel::SetMinValue(const float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MinValue,NewValue))
	{
		
	}
}

void UScalarSettingViewModel::SetStepValue(const float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(StepValue,NewValue))
	{
		
	}
}
