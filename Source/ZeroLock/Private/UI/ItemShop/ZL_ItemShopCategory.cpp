//Copyright Preetham Mukundan (C) 2026


#include "UI/ItemShop/ZL_ItemShopCategory.h"

#include "CommonTextBlock.h"
#include "CommonTileView.h"
#include "Chaos/ChaosPerfTest.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Items/Zero_Item_data.h"
#include "UI/ItemShop/ZL_ITemIcon.h"
#include "ZeroLock/ZeroLock.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UZL_ItemShopCategory::AddItemToTier(UZero_Item_data* itemData)
{
	switch (itemData->ItemTierType)
	{
	case EZeroLockItemTier::Tier1:
			addtoLists(itemData,Tier1_Items,Tier1_ItemListView);
		break;
		case EZeroLockItemTier::Tier2:
			addtoLists(itemData,Tier2_Items,Tier2_ItemListView);
		break;
		case EZeroLockItemTier::Tier3:
			addtoLists(itemData,Tier3_Items,Tier3_ItemListView);
		break;
		case EZeroLockItemTier::Tier4:
		case EZeroLockItemTier::Tier5:
			addtoLists(itemData,Tier4_Items,Tier4_ItemListView);
		break;
		default:
		    addtoLists(itemData,Tier1_Items,Tier1_ItemListView);
	}
}

void UZL_ItemShopCategory::addtoLists(UZero_Item_data* itemData, TArray<UZero_Item_data*>& items,
	class UCommonTileView*& listToAdd)
{
	items.AddUnique(itemData);
	listToAdd->AddItem(itemData);
}

void UZL_ItemShopCategory::OnItemSelected(UObject* Object)
{
	if (UZero_Item_data* ItemData = Cast<UZero_Item_data>(Object))
	{
		OnItemClickedOn.Broadcast(ItemData);
	}
}

void UZL_ItemShopCategory::RefreshIcons(const UZero_Item_Inventory_Component* Inventory)
{
	for (const UCommonTileView* TileView : { Tier1_ItemListView, Tier2_ItemListView, Tier3_ItemListView, Tier4_ItemListView })
	{
		for (UUserWidget* Entry : TileView->GetDisplayedEntryWidgets())
		{
			if (UZL_ITemIcon* Icon = Cast<UZL_ITemIcon>(Entry))
			{
				Icon->RefreshState(Inventory);
			}
		}
	}
}

void UZL_ItemShopCategory::AddDelegates()
{
	Tier1_ItemListView->OnItemClicked().AddUObject(this,&UZL_ItemShopCategory::OnItemSelected);
	Tier2_ItemListView->OnItemClicked().AddUObject(this,&UZL_ItemShopCategory::OnItemSelected);
	Tier3_ItemListView->OnItemClicked().AddUObject(this,&UZL_ItemShopCategory::OnItemSelected);
	Tier4_ItemListView->OnItemClicked().AddUObject(this,&UZL_ItemShopCategory::OnItemSelected);
}

void UZL_ItemShopCategory::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	AddDelegates();
}
