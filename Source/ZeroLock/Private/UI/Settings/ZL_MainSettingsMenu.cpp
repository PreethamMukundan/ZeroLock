// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/ZL_MainSettingsMenu.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "CommonButtonBase.h"
#include "CommonWidgetCarousel.h"
#include "CommonWidgetCarouselNavBar.h"
#include "Components/ScrollBox.h"
#include "UI/Settings/ZL_BTN_SubMenu.h"
#include "UI/Settings/ZL_NamedCarouselNavBar.h"
#include "UI/Settings/ZL_SettingsSubMenu.h"
#include "UI/MVVM/Input/ZL_KeyBindingScreen.h"
#include "ZeroLock/ZeroLock.h"

void UZL_MainSettingsMenu::GenerateTabsFromMap(const TMap<FName, USubMenuManagerViewModel*>& ViewModelMap)
{
	if (!SubMenuBtnClass || !SubMenuWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Missing sub classes for btn or widget"));
		return;
	}
	SubMenuCarousel->ClearChildren();
	for (const TPair<FName, USubMenuManagerViewModel*>& Pair : ViewModelMap)
	{
		FName TabName = Pair.Key;
		USubMenuManagerViewModel* ChildVM = Pair.Value;
		ZLOG(TabName.ToString());
		//SubMenuNavBar->SetLinkedCarousel(SubMenuCarousel);
		UZL_SettingsSubMenu* NewContentWidget = CreateWidget<UZL_SettingsSubMenu>(this, SubMenuWidgetClass);
		if (NewContentWidget)
		{
			NewContentWidget->SubMenuName = TabName;
			NewContentWidget->SetupVM(ChildVM);
			SubMenuCarousel->AddChild(NewContentWidget);
		}
		
	}
	if (KeyBindingScreen)
	{
		UZL_KeyBindingScreen* KeyScreen = CreateWidget<UZL_KeyBindingScreen>(this, KeyBindingScreen);
		if (KeyScreen)
		{
			KeyScreen->SubMenuName = FName("KeyBinding");
			SubMenuCarousel->AddChild(KeyScreen);
		}
	}
	
	SubMenuNavBar->SetLinkedCarousel(SubMenuCarousel);

}

void UZL_MainSettingsMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);

	if (CloseButton)
	{
		CloseButton->OnClicked().AddUObject(this, &UZL_MainSettingsMenu::HandleCloseClicked);
		if (UZL_BTN_SubMenu* CloseTab = Cast<UZL_BTN_SubMenu>(CloseButton))
		{
			CloseTab->SetBTNNAme(FName("Close"));
		}
	}
}

UWidget* UZL_MainSettingsMenu::NativeGetDesiredFocusTarget() const
{
	return const_cast<UZL_MainSettingsMenu*>(this);
}

FReply UZL_MainSettingsMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Rows and the key selector consume keys they use (a key selector waiting for input captures K), so this only sees unhandled ones.
	if (ToggleKey.IsValid() && InKeyEvent.GetKey() == ToggleKey && !InKeyEvent.IsRepeat() && IsActivated())
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UZL_MainSettingsMenu::HandleCloseClicked()
{
	CloseMenu();
}

void UZL_MainSettingsMenu::CloseMenu()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

TOptional<FUIInputConfig> UZL_MainSettingsMenu::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UZL_MainSettingsMenu::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
	}

	// BP_ZeroPlayerController's SetInputModeUIOnly may run before this; make sure the menu holds focus.
	SetFocus();
}

void UZL_MainSettingsMenu::NativeOnDeactivated()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
	}

	Super::NativeOnDeactivated();
}
