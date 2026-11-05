// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZL_ImbuePicker.generated.h"

class UTextBlock;
class UVerticalBox;
class UZL_RoundImage;
class UZero_Item_Inventory_Component;
class UZero_Item_data;

USTRUCT()
struct FZL_ImbueChoice
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UVerticalBox> Cell;

	UPROPERTY()
	TObjectPtr<UZL_RoundImage> Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> Name;

	int32 InputID = 0;
};

/**
 * Box in the middle of the screen for choosing which ability an imbue item goes on, shown by the shop before it buys one. Built in C++. A click on
 * an ability buys the item, a click anywhere else cancels.
 */
UCLASS()
class ZEROLOCK_API UZL_ImbuePicker : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(UZero_Item_data* Item, UZero_Item_Inventory_Component* Inventory);

	UPROPERTY(EditDefaultsOnly, Category = "Imbue")
	float IconSize = 96.0f;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void Build();

	UPROPERTY()
	TArray<FZL_ImbueChoice> Choices;

	UPROPERTY()
	TObjectPtr<UZero_Item_data> PendingItem;

	UPROPERTY()
	TObjectPtr<UZero_Item_Inventory_Component> PendingInventory;
};
