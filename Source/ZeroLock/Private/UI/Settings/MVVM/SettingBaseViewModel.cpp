// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SettingBaseViewModel.h"

void USettingBaseViewModel::SetInteractionType(const ESettingInteractionType XInteractionType)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(InteractionType,XInteractionType))
	{
		
	}
}

void USettingBaseViewModel::SetDisplayName(const FText& XDisplayName)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(DisplayName,XDisplayName))
	{
		
	}
}
