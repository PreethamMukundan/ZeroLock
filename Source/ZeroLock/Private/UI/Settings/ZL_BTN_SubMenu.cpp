// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/ZL_BTN_SubMenu.h"

#include "CommonActivatableWidget.h"
#include "CommonActivatableWidgetSwitcher.h"
#include "CommonTextBlock.h"
#include "CommonWidgetCarousel.h"
#include "CommonWidgetCarouselNavBar.h"
#include "UI/Settings/ZL_SettingsSubMenu.h"


void UZL_BTN_SubMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
}

void UZL_BTN_SubMenu::InitializeTab(FName TabName, UCommonActivatableWidget* WidtoSwit,
                                    UCommonActivatableWidgetSwitcher* LinkedSwitcher)
{
	TargetSwitcher = LinkedSwitcher;
	WidgetToSwitchTO = WidtoSwit;
	if (BTNText)
	{
		BTNText->SetText(FText::FromName(TabName));
	}
}

void UZL_BTN_SubMenu::NativeOnClicked()
{
	Super::NativeOnClicked();
	
	if (TargetSwitcher && WidgetToSwitchTO)
	{
		TargetSwitcher->SetActiveWidget(WidgetToSwitchTO);
	}
}

void UZL_BTN_SubMenu::SetBTNNAme(FName TabName)
{
	if (BTNText)
	{
		BTNText->SetText(FText::FromName(TabName));
	}
}
