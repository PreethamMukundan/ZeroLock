//Copyright Preetham Mukundan (C) 2026


#include "UI/ItemShop/ZL_ItemTooltipWidget.h"

#include "CommonBorder.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Items/Zero_Item_data.h"

void UZL_ItemTooltipWidget::SetupFromItem(class UZero_Item_data* ItemData)
{
	if (!ItemData)
	{
		return;
	}

	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(ItemData->Icon);
	}
	if (ItemNameText)
	{
		ItemNameText->SetText(ItemData->GetDisplayName());
	}
    
	if (DescriptionText)
	{
		DescriptionText->SetText(FText::FromString(ItemData->Description));
	}

	if (ItemData->ItemType == EZeroLockItemType::Spirit)
	{
		if (SpiritBorderStyle)
		{
			BackgroundBorder->SetStyle(SpiritBorderStyle);
		}
	}
	else if (ItemData->ItemType == EZeroLockItemType::Vitality)
	{
		if (VitalityBorderStyle)
		{
			BackgroundBorder->SetStyle(VitalityBorderStyle);
		}
	}
	else
	{
		if (WeaponBorderStyle)
		{
			BackgroundBorder->SetStyle(WeaponBorderStyle);
		}
	}
}
