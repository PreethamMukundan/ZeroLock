//Copyright Preetham Mukundan (C) 2026


#include "UI/ItemShop/ZL_ItemShop_Base.h"
#include "UI/ItemShop/ZL_ImbuePicker.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "CommonButtonBase.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "CommonTileView.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Items/Zero_Item_data.h"
#include "Mover/ZeroMoverPawn.h"
#include "TimerManager.h"
#include "UI/ItemShop/ZL_ITemIcon.h"
#include "UI/ItemShop/ZL_ItemShopCategory.h"
#include "ZeroLock/ZeroLock.h"

UZL_ItemShop_Base::UZL_ItemShop_Base()
{
}

void UZL_ItemShop_Base::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	LoadItemsAsync();
	AddDelegates();
	ItemSwitcher->SetActiveWidget(ItemListView);
}

void UZL_ItemShop_Base::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshIcons();

	// WBP_PlayerHUD sets UI-only input around the same time it activates this, so wait for that to be done.
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActivated())
		{
			AllowKeysWithoutHeroControls();
		}
	}));
}

void UZL_ItemShop_Base::NativeOnDeactivated()
{
	if (ImbuePicker)
	{
		ImbuePicker->RemoveFromParent();
	}

	if (AZeroMoverPawn* Pawn = Cast<AZeroMoverPawn>(GetOwningPlayerPawn()))
	{
		Pawn->SetGameplayInputEnabled(true);
	}
	Super::NativeOnDeactivated();
}

void UZL_ItemShop_Base::AllowKeysWithoutHeroControls()
{
	// The shop closes on WBP_PlayerHUD's B key event, which UI-only input stops at the viewport.
	const APlayerController* PC = GetOwningPlayer();
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (LocalPlayer && LocalPlayer->ViewportClient)
	{
		LocalPlayer->ViewportClient->SetIgnoreInput(false);
	}
	if (AZeroMoverPawn* Pawn = Cast<AZeroMoverPawn>(GetOwningPlayerPawn()))
	{
		Pawn->SetGameplayInputEnabled(false);
	}
}

void UZL_ItemShop_Base::LoadItemsAsync()
{
	if (!ItemTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemTable is NULL"));
		return;
	}
	
	TArray<FItemTableRow*> Rows;
	ItemTable->GetAllRows(TEXT("LoadingItems"), Rows);

	if (Rows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No rows in ItemTable"));
		return;
	}
	
	TArray<FSoftObjectPath> Paths;
	for (FItemTableRow* Row : Rows)
	{
		if (Row && Row->ItemAsset.IsValid())
		{
			LoadedItems.AddUnique(Row->ItemAsset.Get());
		}
		else if (Row && !Row->ItemAsset.IsNull())
		{
			Paths.Add(Row->ItemAsset.ToSoftObjectPath());
		}
	}

	
	if (Paths.Num() == 0)
	{
		OnItemsLoaded();
		return;
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	Streamable.RequestAsyncLoad(Paths,FStreamableDelegate::CreateUObject(this, &UZL_ItemShop_Base::OnItemsLoaded));
}

void UZL_ItemShop_Base::OnItemsLoaded()
{
	ZLOG("Async load all done");
	LoadedItems.Empty();

	TArray<FItemTableRow*> Rows;
	ItemTable->GetAllRows(TEXT("FinishLoading"), Rows);

	for (FItemTableRow* Row : Rows)
	{
		if (!Row || Row->ItemAsset.IsNull()) continue;

		UZero_Item_data* Item = Row->ItemAsset.Get();
		if (Item)
		{
			LoadedItems.AddUnique(Item);
			AddtoCategory(Item);
		}
	}

	if (ItemListView)
		ItemListView->SetListItems(LoadedItems);
	
}

void UZL_ItemShop_Base::BTN_All_Pressed() const
{
	ItemSwitcher->SetActiveWidget(ItemListView);
}

void UZL_ItemShop_Base::BTN_Weapon_Pressed() const
{
	ItemSwitcher->SetActiveWidget(WeaponCategory);
}

void UZL_ItemShop_Base::BTN_Spirit_Pressed() const
{
	ItemSwitcher->SetActiveWidget(SpiritCategory);
}

void UZL_ItemShop_Base::BTN_Vitality_Pressed() const
{
	ItemSwitcher->SetActiveWidget(VitalityCategory);
}


void UZL_ItemShop_Base::AddDelegates()
{
	BTN_All->OnClicked().AddUObject(this,&ThisClass::BTN_All_Pressed);
	BTN_Weapon->OnClicked().AddUObject(this,&ThisClass::BTN_Weapon_Pressed);
	BTN_Spirit->OnClicked().AddUObject(this,&ThisClass::BTN_Spirit_Pressed);
	BTN_Vitality->OnClicked().AddUObject(this,&ThisClass::BTN_Vitality_Pressed);

	SpiritCategory->OnItemClickedOn.AddDynamic(this,&UZL_ItemShop_Base::ItemPressed);
	WeaponCategory->OnItemClickedOn.AddDynamic(this,&UZL_ItemShop_Base::ItemPressed);
	VitalityCategory->OnItemClickedOn.AddDynamic(this,&UZL_ItemShop_Base::ItemPressed);
	ItemListView->OnItemClicked().AddUObject(this,&ThisClass::OnAllListItemClicked);
}

void UZL_ItemShop_Base::OnAllListItemClicked(UObject* Object)
{
	ItemPressed(Cast<UZero_Item_data>(Object));
}

void UZL_ItemShop_Base::ItemPressed(UZero_Item_data* itemData)
{
	UZero_Item_Inventory_Component* Inventory = GetHeroInventory();
	if (!Inventory || !itemData) return;

	// Icons change when the server's answer replicates, so a refused purchase never shows as owned.
	if (Inventory->HasItem(itemData))
	{
		Inventory->ServerSellItem(itemData);
	}
	else if (Inventory->IsUpgradedAway(itemData))
	{
		return;
	}
	else if (Inventory->NeedsImbueChoice(itemData))
	{
		if (!ImbuePicker)
		{
			ImbuePicker = CreateWidget<UZL_ImbuePicker>(this, UZL_ImbuePicker::StaticClass());
		}
		ImbuePicker->AddToViewport(1000);
		ImbuePicker->Open(itemData, Inventory);
	}
	else
	{
		Inventory->ServerBuyItem(itemData, nullptr);
	}
}

UZero_Item_Inventory_Component* UZL_ItemShop_Base::GetHeroInventory()
{
	const APawn* Pawn = GetOwningPlayerPawn();
	UZero_Item_Inventory_Component* Inventory = Pawn ? Pawn->FindComponentByClass<UZero_Item_Inventory_Component>() : nullptr;
	if (Inventory != HeroItemComponent)
	{
		if (IsValid(HeroItemComponent))
		{
			HeroItemComponent->ItemDelegate.RemoveDynamic(this, &ThisClass::OnInventoryChanged);
		}
		HeroItemComponent = Inventory;
		if (Inventory)
		{
			Inventory->ItemDelegate.AddUniqueDynamic(this, &ThisClass::OnInventoryChanged);
		}
	}
	return HeroItemComponent;
}

void UZL_ItemShop_Base::OnInventoryChanged(TArray<FZeroInventoryItem> InventoryItems)
{
	RefreshIcons();
}

void UZL_ItemShop_Base::RefreshIcons()
{
	const UZero_Item_Inventory_Component* Inventory = GetHeroInventory();
	if (!Inventory) return;

	for (UUserWidget* Entry : ItemListView->GetDisplayedEntryWidgets())
	{
		if (UZL_ITemIcon* Icon = Cast<UZL_ITemIcon>(Entry))
		{
			Icon->RefreshState(Inventory);
		}
	}
	SpiritCategory->RefreshIcons(Inventory);
	WeaponCategory->RefreshIcons(Inventory);
	VitalityCategory->RefreshIcons(Inventory);
}

void UZL_ItemShop_Base::AddtoCategory(UZero_Item_data* dataItem)
{
	if (dataItem->ItemType == EZeroLockItemType::Spirit)
	{
		SpiritCategory->AddItemToTier(dataItem);
		return;
	}
	else if (dataItem->ItemType == EZeroLockItemType::Vitality)
	{
		VitalityCategory->AddItemToTier(dataItem);
		return;
	}
	else
	{
		WeaponCategory->AddItemToTier(dataItem);
		return;
	}
	
}

void UZL_ItemShop_Base::ActiveSetForSwitcher(UWidget* WidgettoActive)
{
	if (!WidgettoActive)return;
	ItemSwitcher->SetActiveWidget(WidgettoActive);
}
