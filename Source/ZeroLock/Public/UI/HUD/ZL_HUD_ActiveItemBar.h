// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZL_HUD_ActiveItemBar.generated.h"

class UTextBlock;
class UZL_RoundImage;
class UZero_Item_Inventory_Component;
class UZero_Item_data;

USTRUCT()
struct FZL_ActiveItemSlotWidgets
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UZL_RoundImage> Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CooldownText;

	UPROPERTY()
	TObjectPtr<const UZero_Item_data> ShownItem;
};

/**
 * One round slot per active item key: the item's icon, its key and a radial cooldown. Built in C++, placed in WBP_PlayerHUD
 * as ActiveItemBar: move it there like any other widget. Polls the owning pawn, so a hero swap needs no rebinding. Hidden with no active items.
 * While the cursor is out (Tab held), items drag between keys; dropping on another item swaps them.
 */
UCLASS()
class ZEROLOCK_API UZL_HUD_ActiveItemBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Active Items")
	float SlotSize = 64.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Active Items")
	float SlotSpacing = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Active Items")
	FLinearColor EmptySlotColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.35f);

	/** Opacity of the item left behind while it's being dragged. */
	UPROPERTY(EditDefaultsOnly, Category = "Active Items")
	float DraggedSlotOpacity = 0.35f;

protected:
	virtual void NativeOnInitialized() override;

	/** Builds the slots here too, so the bar shows in the widget editor, where NativeOnInitialized doesn't run. */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	void BuildSlots();
	void ShowItem(int32 ItemSlot, const UZero_Item_data* Item);
	void RefreshKeys();
	void UpdateCooldown(int32 ItemSlot, const UZero_Item_Inventory_Component* Inventory, const class UAbilitySystemComponent* ASC);

	/** The slot under the pointer, INDEX_NONE outside the row. Each slot owns half the gap on either side. */
	int32 FindSlotAt(const FGeometry& Geometry, const FPointerEvent& Event) const;
	void EndDrag();

	/** Transient: slots built in the widget editor must not be saved into the HUD. */
	UPROPERTY(Transient)
	TArray<FZL_ActiveItemSlotWidgets> Slots;

	int32 PressedSlot = INDEX_NONE;
	int32 DragSlot = INDEX_NONE;
};
