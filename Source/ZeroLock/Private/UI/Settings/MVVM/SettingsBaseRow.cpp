// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/MVVM/SettingsBaseRow.h"

#include "UI/Settings/MVVM/SettingBaseViewModel.h"


void USettingsBaseRow::SetViewModel(USettingBaseViewModel* InViewModel)
{
	SetupComponents(InViewModel);
}

void USettingsBaseRow::SetupComponents(USettingBaseViewModel* InViewModel)
{
}

void USettingsBaseRow::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	if (USettingBaseViewModel* VM = Cast<USettingBaseViewModel>(ListItemObject))
	{
		SetViewModel(VM); 
	}
}

void USettingsBaseRow::OnRightClicked()
{
}

void USettingsBaseRow::OnLeftClicked()
{
}

FNavigationReply USettingsBaseRow::NativeOnNavigation(const FGeometry& MyGeometry,
                                                      const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	

	EUINavigation NavDir = InNavigationEvent.GetNavigationType();
	
	if (NavDir == EUINavigation::Right)
	{
		OnRightClicked();
		return FNavigationReply::Stop(); 
	}
	if (NavDir == EUINavigation::Left)
	{
		OnLeftClicked();
		return FNavigationReply::Stop(); 
	}
	if (NavDir == EUINavigation::Up || NavDir == EUINavigation::Down)
	{
		return FNavigationReply::Escape();
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}
