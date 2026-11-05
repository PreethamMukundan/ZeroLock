// Copyright Preetham Mukundan (C) 2026

#include "UI/HUD/ZL_HUD_ActiveItemBar.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Items/ZL_GA_ActiveItem.h"
#include "Items/Zero_Item_Inventory_Component.h"
#include "UI/HUD/ZL_RoundImage.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UZL_HUD_ActiveItemBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// User widgets default to letting the mouse through, and dragging needs it. With the cursor hidden nothing reaches this.
	SetVisibility(ESlateVisibility::Visible);

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildSlots();
	}
}

TSharedRef<SWidget> UZL_HUD_ActiveItemBar::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildSlots();
	}
	return Super::RebuildWidget();
}

void UZL_HUD_ActiveItemBar::BuildSlots()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = Row;

	Slots.SetNum(UZero_Item_Inventory_Component::MaxActiveSlots);
	for (int32 Index = 0; Index < Slots.Num(); Index++)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(SlotSize);
		Size->SetHeightOverride(SlotSize);
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(Size))
		{
			RowSlot->SetPadding(FMargin(SlotSpacing * 0.5f, 0.0f));
		}

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Size->AddChild(Stack);

		FZL_ActiveItemSlotWidgets& Widgets = Slots[Index];
		Widgets.Icon = WidgetTree->ConstructWidget<UZL_RoundImage>(UZL_RoundImage::StaticClass());
		Widgets.Icon->OutlineThickness = 2.0f;
		Widgets.Icon->SetColorAndOpacity(EmptySlotColor);
		if (UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Widgets.Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
		}

		Widgets.CooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Widgets.CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* TimeSlot = Stack->AddChildToOverlay(Widgets.CooldownText))
		{
			TimeSlot->SetHorizontalAlignment(HAlign_Center);
			TimeSlot->SetVerticalAlignment(VAlign_Center);
		}

		Widgets.KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo KeyFont = Widgets.KeyText->GetFont();
		KeyFont.Size = 12;
		Widgets.KeyText->SetFont(KeyFont);
		if (UOverlaySlot* KeySlot = Stack->AddChildToOverlay(Widgets.KeyText))
		{
			KeySlot->SetHorizontalAlignment(HAlign_Center);
			KeySlot->SetVerticalAlignment(VAlign_Bottom);
		}
	}
}

void UZL_HUD_ActiveItemBar::ShowItem(int32 ItemSlot, const UZero_Item_data* Item)
{
	FZL_ActiveItemSlotWidgets& Widgets = Slots[ItemSlot];
	Widgets.ShownItem = Item;

	if (Item && Item->Icon)
	{
		Widgets.Icon->SetBrushFromTexture(Item->Icon);
		Widgets.Icon->SetColorAndOpacity(FLinearColor::White);
	}
	else
	{
		Widgets.Icon->SetBrushFromTexture(nullptr);
		Widgets.Icon->SetColorAndOpacity(EmptySlotColor);
		Widgets.Icon->SetRadialProgress(1.0f);
		Widgets.CooldownText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UZL_HUD_ActiveItemBar::RefreshKeys()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwningPlayerPawn());
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Hero || !Input)
	{
		return;
	}

	// Asked here rather than hard-coded, so a remapped key shows. Empty slots show theirs too, as somewhere to drag to.
	for (int32 ItemSlot = 0; ItemSlot < Slots.Num(); ItemSlot++)
	{
		FText KeyName;
		const UInputAction* Action = Hero->ItemInputActions.IsValidIndex(ItemSlot) ? Hero->ItemInputActions[ItemSlot].Get() : nullptr;
		if (Action)
		{
			const TArray<FKey> Keys = Input->QueryKeysMappedToAction(Action);
			if (!Keys.IsEmpty())
			{
				KeyName = Keys[0].GetDisplayName(false);
			}
		}
		Slots[ItemSlot].KeyText->SetText(KeyName);
	}
}

void UZL_HUD_ActiveItemBar::UpdateCooldown(int32 ItemSlot, const UZero_Item_Inventory_Component* Inventory, const UAbilitySystemComponent* ASC)
{
	FZL_ActiveItemSlotWidgets& Widgets = Slots[ItemSlot];

	float Remaining = 0.0f;
	float Duration = 0.0f;
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ZLActiveItem::GetCooldownTag(Inventory->GetActiveCooldownSlot(ItemSlot))));
	for (const TPair<float, float>& Time : ASC->GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (Time.Key > Remaining)
		{
			Remaining = Time.Key;
			Duration = Time.Value;
		}
	}

	if (Remaining > 0.0f && Duration > 0.0f)
	{
		Widgets.Icon->SetRadialProgress(1.0f - Remaining / Duration);
		Widgets.CooldownText->SetText(FText::AsNumber(FMath::CeilToInt32(Remaining)));
		Widgets.CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Widgets.Icon->SetRadialProgress(1.0f);
		Widgets.CooldownText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UZL_HUD_ActiveItemBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwningPlayerPawn());
	const UZero_Item_Inventory_Component* Inventory = Hero ? Hero->GetInventoryComponent() : nullptr;
	const UAbilitySystemComponent* ASC = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	if (!Inventory || !ASC)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	if (DragSlot != INDEX_NONE && PC && !PC->bShowMouseCursor)
	{
		UWidgetBlueprintLibrary::CancelDragDrop();
	}

	bool bAnyItem = false;
	bool bChanged = false;
	for (int32 ItemSlot = 0; ItemSlot < Slots.Num(); ItemSlot++)
	{
		const UZero_Item_data* Item = Inventory->GetActiveItem(ItemSlot);
		if (Item != Slots[ItemSlot].ShownItem)
		{
			ShowItem(ItemSlot, Item);
			bChanged = true;
		}
		if (Item)
		{
			bAnyItem = true;
			UpdateCooldown(ItemSlot, Inventory, ASC);
		}
	}

	if (bChanged)
	{
		RefreshKeys();
	}

	// Ticking stops only for Collapsed, which would hide the first item bought, so the row just goes transparent.
	SetRenderOpacity(bAnyItem ? 1.0f : 0.0f);
}

int32 UZL_HUD_ActiveItemBar::FindSlotAt(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	const float LocalX = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()).X;
	const int32 ItemSlot = FMath::FloorToInt32(LocalX / (SlotSize + SlotSpacing));
	return Slots.IsValidIndex(ItemSlot) ? ItemSlot : INDEX_NONE;
}

FReply UZL_HUD_ActiveItemBar::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const APlayerController* PC = GetOwningPlayer();
	const int32 ItemSlot = FindSlotAt(InGeometry, InMouseEvent);
	if (!PC || !PC->bShowMouseCursor || ItemSlot == INDEX_NONE || !Slots[ItemSlot].ShownItem)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	PressedSlot = ItemSlot;
	return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
}

void UZL_HUD_ActiveItemBar::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	const UZero_Item_data* Item = Slots.IsValidIndex(PressedSlot) ? Slots[PressedSlot].ShownItem.Get() : nullptr;
	if (!Item)
	{
		return;
	}

	UZL_RoundImage* Visual = NewObject<UZL_RoundImage>(this);
	Visual->OutlineThickness = 2.0f;
	Visual->SetBrushFromTexture(Item->Icon);
	Visual->SetDesiredSizeOverride(FVector2D(SlotSize));

	OutOperation = NewObject<UDragDropOperation>(this);
	OutOperation->DefaultDragVisual = Visual;
	OutOperation->Pivot = EDragPivot::CenterCenter;

	DragSlot = PressedSlot;
	Slots[DragSlot].Icon->SetRenderOpacity(DraggedSlotOpacity);
}

bool UZL_HUD_ActiveItemBar::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const int32 FromSlot = DragSlot;
	const int32 ToSlot = FindSlotAt(InGeometry, InDragDropEvent);
	EndDrag();

	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwningPlayerPawn());
	UZero_Item_Inventory_Component* Inventory = Hero ? Hero->GetInventoryComponent() : nullptr;
	if (Inventory && FromSlot != INDEX_NONE && ToSlot != INDEX_NONE && FromSlot != ToSlot)
	{
		Inventory->ServerMoveActiveItem(FromSlot, ToSlot);
	}
	return true;
}

void UZL_HUD_ActiveItemBar::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	EndDrag();
}

void UZL_HUD_ActiveItemBar::EndDrag()
{
	if (Slots.IsValidIndex(DragSlot))
	{
		Slots[DragSlot].Icon->SetRenderOpacity(1.0f);
	}
	DragSlot = INDEX_NONE;
	PressedSlot = INDEX_NONE;
}
