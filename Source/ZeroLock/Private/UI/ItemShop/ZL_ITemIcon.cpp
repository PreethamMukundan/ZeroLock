//Copyright Preetham Mukundan (C) 2026


#include "UI/ItemShop/ZL_ITemIcon.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "CommonBorder.h"
#include "Components/WidgetInteractionComponent.h"
#include "GameFramework/Pawn.h"
#include "Items/Zero_Item_data.h"
#include "Items/Zero_Item_Inventory_Component.h"
#include "UI/ItemShop/ZL_ItemTooltipWidget.h"
#include "ZeroLock/ZeroLock.h"

namespace
{
	constexpr float CornerRadius = 40.0f;

	/** Imbue items curve on top and active items on the bottom, so the shop shows what kind an item is. False for neither. */
	bool ShapeBrush(FSlateBrush& Brush, const UZero_Item_data* Item)
	{
		const float Top = Item && Item->bImbue ? CornerRadius : 0.0f;
		const float Bottom = Item && Item->IsActiveItem() ? CornerRadius : 0.0f;
		if (Top <= 0.0f && Bottom <= 0.0f)
		{
			return false;
		}

		// Drawn as a rounded box, the brush clips its texture to the shape. Corners run clockwise from the top left.
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Top, Top, Bottom, Bottom);
		Brush.OutlineSettings.Width = 0.0f;
		return true;
	}
}

void UZL_ITemIcon::SetupFromItem(class UZero_Item_data* ItemData)
{
	if (!ItemData)return;
	ItemDataToStore = ItemData; 
	ItemName->SetText(ItemData->GetDisplayName());
	ItemIcon->SetBrushFromTexture(ItemData->Icon);

	// List entries get reused, so an item with no shape has to clear the last one's.
	FSlateBrush IconBrush = ItemIcon->GetBrush();
	if (!ShapeBrush(IconBrush, ItemData))
	{
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	ItemIcon->SetBrush(IconBrush);
}

void UZL_ITemIcon::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	UZero_Item_data* ItemData = Cast<UZero_Item_data>(ListItemObject);
	if (!ItemData) return;

	SetupFromItem(ItemData);

	if (TooltipWidgetClass)
	{
		UZL_ItemTooltipWidget* TooltipWidget = CreateWidget<UZL_ItemTooltipWidget>(GetOwningPlayer(),TooltipWidgetClass	);
		if (TooltipWidget)
		{
			TooltipWidget->SetupFromItem(ItemData);
			SetToolTip(TooltipWidget);
		}
	}

	// List entries get reused for other items, so this one's state starts over.
	SetItemCanBeUpgradedTo(false, nullptr);
	SetOnItemSold();
	if (const APawn* Pawn = GetOwningPlayerPawn())
	{
		RefreshState(Pawn->FindComponentByClass<UZero_Item_Inventory_Component>());
	}
}

void UZL_ITemIcon::RefreshState(const UZero_Item_Inventory_Component* Inventory)
{
	if (!Inventory || !ItemDataToStore) return;

	const TArray<UZero_Item_data*> Components = Inventory->GetOwnedComponents(ItemDataToStore);
	UZero_Item_data* UpgradeFrom = Components.IsEmpty() ? nullptr : Components[0];

	EItemState NewState = EItemState::Default;
	if (Inventory->HasItem(ItemDataToStore))
	{
		NewState = EItemState::Sold;
	}
	else if (Inventory->IsUpgradedAway(ItemDataToStore))
	{
		NewState = EItemState::Blocked;
	}
	else if (UpgradeFrom)
	{
		NewState = EItemState::ReadyToUpgrade;
	}

	if (NewState == ItemCurrentState && UpgradeFrom == ItemUpgradedFrom) return;

	if (ItemCurrentState == EItemState::ReadyToUpgrade)
	{
		SetItemCanBeUpgradedTo(false, nullptr);
	}
	SetOnItemSold();

	if (NewState == EItemState::Sold)
	{
		SetOnItemPurchased();
	}
	else if (NewState == EItemState::Blocked)
	{
		SetUpgradedBlocked();
	}
	else if (NewState == EItemState::ReadyToUpgrade)
	{
		SetItemCanBeUpgradedTo(true, UpgradeFrom);
	}
}


void UZL_ITemIcon::SetOnItemPurchased()
{
	ItemIcon->SetOpacity(0.5f);
	ItemCurrentState = EItemState::Sold;

	ItemStates->SetText(FText::FromString("Sold"));
	ItemStates->SetVisibility(ESlateVisibility::Visible);
}

void UZL_ITemIcon::SetOnItemSold()
{
	ItemStates->SetVisibility(ESlateVisibility::Collapsed);
	ItemIcon->SetOpacity(1.0f);
	ItemCurrentState = EItemState::Default;
}

void UZL_ITemIcon::SetItemCanBeUpgradedTo(bool bCnaBeUpgraded, UZero_Item_data* ItemFrom)
{
	if (ItemCurrentState == EItemState::Sold)return;
	if (bCnaBeUpgraded)
	{
		ItemStates->SetText(FText::FromString("Upgraded"));
		ItemStates->SetVisibility(ESlateVisibility::Visible);
		ItemCurrentState = EItemState::ReadyToUpgrade;
		OnAnimationPlay(true);
		ItemUpgradedFrom = ItemFrom;
	}
	else
	{
		ItemStates->SetVisibility(ESlateVisibility::Collapsed);
		ItemCurrentState = EItemState::Default;
		OnAnimationPlay(false);
		ItemUpgradedFrom = ItemFrom;
	}
	
}

void UZL_ITemIcon::SetUpgradedBlocked()
{
	ItemIcon->SetOpacity(0.5f);
	ItemCurrentState = EItemState::Blocked;
}

void UZL_ITemIcon::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	SetRenderScale(FVector2D(1.2,1.2));
	SetRenderTranslation({-5, -5});
	if (!NoneHoverStyle)
	{
		NoneHoverStyle=BackGroundCommon->Style;
	}
	if (HoverCBStyle)
	{
		BackGroundCommon->SetStyle(HoverCBStyle);
	}
}

void UZL_ITemIcon::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	SetRenderScale(FVector2D(1,1));
	SetRenderTranslation({0, 0});
	if (NoneHoverStyle)
	{
		BackGroundCommon->SetStyle(NoneHoverStyle);
	}
}
