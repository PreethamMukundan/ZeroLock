// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/BooleanSettingViewModel.h"

#include "UI/Settings/BooleanSettingHandler.h"

void UBooleanSettingViewModel::ApplyToggle(bool bNewState)
{
	if (UBooleanSettingHandler* BoolHandler = Cast<UBooleanSettingHandler>(BaseHandler))
	{
		BoolHandler->OnApplyToggle(bNewState);
	}
}

void UBooleanSettingViewModel::SetbIsToggled(const bool bNewState)
{

	if (UE_MVVM_SET_PROPERTY_VALUE(bIsToggled,bNewState))
	{
		ApplyToggle(bIsToggled);
	}
}
