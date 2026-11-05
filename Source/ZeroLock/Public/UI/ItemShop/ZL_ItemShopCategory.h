//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Items/Zero_Item_Inventory_Component.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "ZL_ItemShopCategory.generated.h"

class UZL_ITemIcon;
class UZero_Item_data;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemClickedOn ,UZero_Item_data* ,ItemClickedOn);


UCLASS()
class ZEROLOCK_API UZL_ItemShopCategory : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UCommonTileView* Tier1_ItemListView;
	UPROPERTY(meta = (BindWidget))
	class UCommonTileView* Tier2_ItemListView;
	UPROPERTY(meta = (BindWidget))
	class UCommonTileView* Tier3_ItemListView;
	UPROPERTY(meta = (BindWidget))
	class UCommonTileView* Tier4_ItemListView;

	UFUNCTION()
	void AddItemToTier(UZero_Item_data* itemData);

	UPROPERTY()
	TArray<UZero_Item_data*> Tier1_Items;
	UPROPERTY()
	TArray<UZero_Item_data*> Tier2_Items;
	UPROPERTY()
	TArray<UZero_Item_data*> Tier3_Items;
	UPROPERTY()
	TArray<UZero_Item_data*> Tier4_Items;

	void addtoLists(UZero_Item_data* itemData, TArray<UZero_Item_data*>& items,class UCommonTileView*& listToAdd);


	void OnItemSelected(UObject* Object);

	/** Updates the icons on screen; the rest pick it up when they are generated. */
	void RefreshIcons(const UZero_Item_Inventory_Component* Inventory);

	void AddDelegates();
	virtual void NativeOnInitialized() override;


	UPROPERTY(BlueprintAssignable)
	FOnItemClickedOn OnItemClickedOn;

	//void SetPostionOfPopUP(FVector2D postion);

	
};
