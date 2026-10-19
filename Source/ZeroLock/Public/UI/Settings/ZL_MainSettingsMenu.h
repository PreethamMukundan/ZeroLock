// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InputCoreTypes.h"
#include "ZL_MainSettingsMenu.generated.h"

class UZL_KeyBindingScreen;
class UInputAction;
class UScrollBox;
class UCommonActivatableWidgetSwitcher;
class USubMenuManagerViewModel;
class UZL_SettingsSubMenu;
class UZL_BTN_SubMenu;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_MainSettingsMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	TSubclassOf<UZL_BTN_SubMenu> SubMenuBtnClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	TSubclassOf<UZL_SettingsSubMenu> SubMenuWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	TSubclassOf<UZL_KeyBindingScreen> KeyBindingScreen;
	
	UFUNCTION(BlueprintCallable, Category = "UI Logic")
	void GenerateTabsFromMap(const TMap<FName, USubMenuManagerViewModel*>& ViewModelMap);

	/** Same key that opens the menu (BP_ZeroPlayerController) closes it. The controller doesn't get it while the menu has Menu input. */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	FKey ToggleKey = EKeys::K;

	UFUNCTION(BlueprintCallable, Category = "UI Logic")
	void CloseMenu();

protected:
	virtual void NativeOnInitialized() override;
	/** Focus lands on the menu when it opens so its key events (ToggleKey) reach NativeOnKeyDown instead of the game viewport. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleCloseClicked();

	/** Optional; any CommonButton. A UZL_BTN_SubMenu gets its label set to "Close". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UCommonButtonBase> CloseButton;

	/** Menu input with a free cursor while open; CommonUI restores the layout's game config on deactivate. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UCommonWidgetCarousel* SubMenuCarousel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UZL_NamedCarouselNavBar* SubMenuNavBar;
};
