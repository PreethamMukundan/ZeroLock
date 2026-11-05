//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonBorder.h"
#include "CommonButtonBase.h"
#include "CommonUserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Components/WidgetComponent.h"
#include "ZL_ITemIcon.generated.h"


UENUM(BlueprintType)
enum class EItemState : uint8
{
	Default UMETA(DisplayName="Default"),
	Sold UMETA(DisplayName="Sold"),
	ReadyToUpgrade UMETA(DisplayName="ReadyToUpgrade"),
	Blocked UMETA(DisplayName="Blocked"),
};
class UImage;
class UCommonTextBlock;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_ITemIcon : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* ItemName;

	UPROPERTY(meta = (BindWidget))
	UImage* ItemIcon;
	UPROPERTY(meta = (BindWidget))
	class UCommonBorder* BackGroundCommon;
	

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* ItemStates;



	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UCommonBorderStyle> HoverCBStyle;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UCommonBorderStyle> NoneHoverStyle;


	

	UFUNCTION(BlueprintCallable)
	void SetupFromItem(class UZero_Item_data* ItemData);

	/** Shows what the inventory says about this item. */
	void RefreshState(const class UZero_Item_Inventory_Component* Inventory);

	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UZero_Item_data* ItemDataToStore;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UZero_Item_data* ItemUpgradedFrom;

	
	UFUNCTION(Blueprintable,BlueprintImplementableEvent)
	void OnAnimationPlay(bool activate);
	
	UFUNCTION(BlueprintCallable)
	void SetOnItemPurchased();
	UFUNCTION(BlueprintCallable)
	void SetOnItemSold();
	UFUNCTION(BlueprintCallable)
	void SetItemCanBeUpgradedTo(bool bCnaBeUpgraded , UZero_Item_data* ItemFrom);

	UFUNCTION(BlueprintCallable)
	void SetUpgradedBlocked();
	 
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemState ItemCurrentState;
	
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip")
	TSubclassOf<class UZL_ItemTooltipWidget> TooltipWidgetClass;
	
};
