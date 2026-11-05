//Copyright Preetham Mukundan (C) 2026


#include "UI/ItemShop/ZL_Hero_HUD_Inventory.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Items/Zero_Item_Inventory_Component.h"
#include "Kismet/GameplayStatics.h"
#include "UI/ItemShop/ZL_ITemIcon.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UZL_Hero_HUD_Inventory::ItemsChanged(TArray<FZeroInventoryItem> InventoryItem)
{
	int32 num =0;
	for (UZL_ITemIcon* itSlot :Slots)
	{
		if (DefaultItem)
		{
			itSlot->SetupFromItem(DefaultItem);
		}
	}
	for (UZL_ITemIcon* itSlot :Slots)
	{
		if (InventoryItem.Num() <= num)
		{
			if (DefaultItem)
			{
				itSlot->SetupFromItem(DefaultItem);
			}
		}
		else
		{
			if (InventoryItem[num].ItemData)
			{
				itSlot->SetupFromItem(InventoryItem[num].ItemData);
			}
		}
		num++;
	}
}

void UZL_Hero_HUD_Inventory::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Slots.Empty();


	
	

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(),0));
	if (Hero)
	{
		Hero->GetInventoryComponent()->ItemDelegate.AddUniqueDynamic(this,&UZL_Hero_HUD_Inventory::ItemsChanged);
		int32 maxslots =Hero->GetInventoryComponent()->MaxSlots;
		if (!ItemIconClass)return;
		for (int32 i = 0; i < maxslots; i++)
		{
			int rowNum = i % 2;
			int  colNum = i / 2;

			UZL_ITemIcon* iconSlot = CreateWidget<UZL_ITemIcon>(this,ItemIconClass);
			if (iconSlot)
			{
				iconSlot->SetupFromItem(DefaultItem);
				Slots.Add(iconSlot);
				UUniformGridSlot* GridSlot = GridTileView->AddChildToUniformGrid(iconSlot,rowNum,colNum);

			}
		}
		ItemsChanged(Hero->GetInventoryComponent()->Items);
	}
}
