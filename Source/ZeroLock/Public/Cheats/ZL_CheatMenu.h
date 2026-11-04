// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "ZL_CheatMenu.generated.h"

class UPanelWidget;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UZL_CheatComponent;
class UZL_Character_Data_Asset;

/** UButton that runs a native callback (the cheat menu builds its buttons in code). */
UCLASS()
class ZEROLOCK_API UZL_CheatButton : public UButton
{
	GENERATED_BODY()

public:
	void SetOnPressed(TFunction<void()> InOnPressed);

private:
	UFUNCTION()
	void HandleClicked();

	TFunction<void()> OnPressed;
};

/**
 * Dev cheat menu, built entirely in C++ (no widget Blueprint needed). AZL_HUDClass pushes it onto UI.Layer.Menu with
 * CheatMenuKey (0); the same key, Esc or Close closes it. Every button calls UZL_CheatComponent on the local player
 * controller. Two tabs:
 *  - Cheats: toggles (god mode, noclip, no cooldowns, infinite ammo), refills, souls, ability levels, teleport, and a
 *    hero selector to spawn an enemy / ally target dummy of that hero (immortal, strafing) or play as it.
 *  - Debug: live mover + network readout (also as an on-screen overlay), Mover / NetworkPrediction debug-draw cvars,
 *    stat commands and network emulation presets.
 */
UCLASS()
class ZEROLOCK_API UZL_CheatMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UZL_CheatMenu(const FObjectInitializer& ObjectInitializer);

	/** Closes the menu (the HUD sets it to its CheatMenuKey). */
	UPROPERTY(EditDefaultsOnly, Category = "Cheats")
	FKey ToggleKey = EKeys::Zero;

	UFUNCTION(BlueprintCallable, Category = "Cheats")
	void CloseMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

private:
	void BuildMenu();
	void BuildCheatsPage();
	void BuildDebugPage();
	UZL_CheatComponent* GetCheats() const;

	/** Adds a scrolling page to the tab switcher and makes it the target of AddHeader / AddRow. */
	void AddPage();

	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color = FLinearColor::White);
	void AddHeader(const FString& Text);
	UPanelWidget* AddRow();
	UZL_CheatButton* AddButton(UPanelWidget* Parent, const FString& Label, TFunction<void()> OnPressed);
	/** Button whose label shows "<Label>: ON/OFF" from IsOn, refreshed every tick. */
	void AddToggle(UPanelWidget* Parent, const FString& Label, TFunction<bool()> IsOn, TFunction<void()> OnPressed);
	/** Toggle for an int/bool console variable on this machine (0 / 1). */
	void AddCVarToggle(UPanelWidget* Parent, const FString& Label, const TCHAR* CVarName);
	/** Text refreshed every tick; highlighted (green) while IsOn returns true, if given. */
	void AddLiveLabel(UTextBlock* Text, TFunction<FString()> GetText, TFunction<bool()> IsOn = nullptr);

	/** Selected hero: 0 = my current hero, then Heroes[i - 1]. */
	TSubclassOf<class AZeroLockCharacter> GetSelectedHeroClass() const;
	FString GetSelectedHeroName() const;

	/** Where AddHeader / AddRow put things. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> Pages;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> FirstButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZL_Character_Data_Asset>> Heroes;

	int32 SelectedHero = 0;

	struct FLiveLabel
	{
		TWeakObjectPtr<UTextBlock> Text;
		TFunction<FString()> GetText;
		TFunction<bool()> IsOn;
	};
	TArray<FLiveLabel> LiveLabels;
};
