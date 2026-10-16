//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InputCoreTypes.h"
#include "ZL_HeroSelectionScreen.generated.h"

class UCommonTileView;
class UZl_CharacterSelectionVM;
class UZL_Character_Data_Asset;
class UZL_CharacterSelectionSubsystem;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_HeroSelectionScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UZL_HeroSelectionScreen();

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	/** Tints the ability icon backgrounds with the hovered hero's IconColor. */
	UFUNCTION()
	void HandleCharacterChanged(UZL_Character_Data_Asset* NewCharacter);

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleOnHoveredChanged(UObject* Item, bool bIsHovered);

	UFUNCTION()
	void HandleOnSelectionChanged(UObject* Item);

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTileView> HeroSelectionList;

	UPROPERTY(BlueprintReadWrite, Category = "MVVM")
	UZl_CharacterSelectionVM* SelectionVM;

	/** In-game swap (AZL_HeroSwapPad): clicking a hero asks the server to swap to it and closes the screen. Also puts input in menu mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroSwap")
	bool bSwapHeroOnClick = false;

	/** With bSwapHeroOnClick: pressing this closes the screen (menu input mode blocks the HUD's H binding, so the screen handles it). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroSwap")
	FKey CloseKey = EKeys::H;

	/** Image widgets behind the 4 ability icons (circles in the BP); tinted with the hovered hero's IconColor. */
	UPROPERTY(EditDefaultsOnly, Category = "HeroSelect")
	TArray<FName> AbilityBackgroundWidgetNames = { TEXT("Image_3"), TEXT("Image_4"), TEXT("Image_5"), TEXT("Image_6") };

	/** Ability background colour while no hero is hovered. */
	UPROPERTY(EditDefaultsOnly, Category = "HeroSelect")
	FLinearColor NoHeroAbilityBackgroundColor = FLinearColor(0.04f, 0.04f, 0.04f, 0.5f);


	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void VM_PopulateList(TArray<UZL_Character_Data_Asset*> DataArray);

private:

	UZL_CharacterSelectionSubsystem* GetSelectionSubsystem() const;
	
};
