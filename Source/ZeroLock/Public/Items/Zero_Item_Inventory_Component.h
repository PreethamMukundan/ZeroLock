//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "Components/ActorComponent.h"
#include "Zero_Item_Inventory_Component.generated.h"

class UGameplayEffect;
class UZero_Item_data;
enum class EZL_ItemStat : uint8;

USTRUCT(BlueprintType)
struct FZeroInventoryItem
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UZero_Item_data* ItemData = nullptr;

	/** Which active item key this item is on (0-3), INDEX_NONE for passive items. */
	UPROPERTY(BlueprintReadOnly)
	int32 ActiveSlot = INDEX_NONE;

	/** The EGASAbilityInputID of the ability an imbue item is on, 0 otherwise. */
	UPROPERTY(BlueprintReadOnly)
	int32 ImbueInputID = 0;

	/** Which cooldown tag this item uses. Fixed for as long as it's owned, so a running cooldown follows it to another key. */
	UPROPERTY(BlueprintReadOnly)
	int32 CooldownSlot = INDEX_NONE;

	/** Handles of applied stat effects. Server only. */
	UPROPERTY(NotReplicated)
	TArray<FActiveGameplayEffectHandle> StatEffectHandles;

	/** Ability handles we grant. Server only. */
	UPROPERTY(NotReplicated)
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZeroInventoryItemDelegate, TArray<FZeroInventoryItem>, InventoryItem);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZEROLOCK_API UZero_Item_Inventory_Component : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UZero_Item_Inventory_Component();
protected:
	virtual void BeginPlay() override;

public:
virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	/** Replicated list of items */
	UPROPERTY(ReplicatedUsing = OnRep_Items, VisibleAnywhere, BlueprintReadOnly)
	TArray<FZeroInventoryItem> Items;

	/** Max inventory slots */
	UPROPERTY(EditDefaultsOnly)
	int32 MaxSlots = 12;

	/** One per item key (EGASAbilityInputID::Item_1 to Item_4). */
	static constexpr int32 MaxActiveSlots = 4;

	/** GAS reference */
	UAbilitySystemComponent* GetASC() const;

	//------------------------------------
	// Buying / Selling
	//------------------------------------

	/** Owned items that upgrade into ItemData are consumed and credited at full cost. The server works those out itself, so ItemUpgradedFrom is ignored. */
	UFUNCTION(BlueprintCallable,Server, Reliable)
	void ServerBuyItem(UZero_Item_data* ItemData , UZero_Item_data* ItemUpgradedFrom);

	UFUNCTION(BlueprintCallable,Server, Reliable)
	void ServerSellItem(UZero_Item_data* ItemData);

	/** Buys an imbue item onto one hero ability. */
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerBuyImbuedItem(UZero_Item_data* ItemData, int32 InputID);

	/** Moves the active item on FromSlot to ToSlot, swapping with whatever is there. */
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerMoveActiveItem(int32 FromSlot, int32 ToSlot);

	/** Server: puts back what a respawn strips, which is every active effect and every running ability. */
	void ReapplyItemEffects();

	//------------------------------------
	// Queries, valid on clients too
	//------------------------------------

	bool HasItem(const UZero_Item_data* ItemData) const;

	/** The item on an active key, null if the key is free. */
	const UZero_Item_data* GetActiveItem(int32 Slot) const;

	/** True when buying ItemData has to ask for an ability: an upgrade keeps the one it replaces. */
	bool NeedsImbueChoice(const UZero_Item_data* ItemData) const;

	/** The ability an owned item is imbued on, 0 if none. */
	int32 GetImbueInput(const UZero_Item_data* ItemData) const;

	/** What imbue items add to the ability with this input ID. */
	float GetImbueBonus(EZL_ItemStat Stat, int32 InputID) const;

	/** The cooldown slot of the item on an active key, INDEX_NONE if the key is free. */
	int32 GetActiveCooldownSlot(int32 Slot) const;

	/** Owned items that upgrade into ItemData. */
	TArray<UZero_Item_data*> GetOwnedComponents(const UZero_Item_data* ItemData) const;

	/** True when something owned was built from ItemData, which then can't be bought again. */
	bool IsUpgradedAway(const UZero_Item_data* ItemData) const;

protected:

	UFUNCTION()
	void OnRep_Items();

	void ApplyItemEffects(FZeroInventoryItem& NewItem);
	void ApplyStatEffects(FZeroInventoryItem& Item);
	void RemoveItemEffects(FZeroInventoryItem& Item);

	/** The lowest key no owned item is on, INDEX_NONE when all four are taken. */
	int32 FindFreeActiveSlot() const;

	/** The lowest cooldown slot no owned item uses. There is always one while a key is free. */
	int32 FindFreeCooldownSlot() const;

	/** Index in Items of the item on an active key, INDEX_NONE if the key is free. */
	int32 FindActiveItemIndex(int32 Slot) const;

	/** Server: puts the item on a key. Only the ability's input changes, so anything it's doing carries on. */
	void SetActiveSlot(FZeroInventoryItem& Item, int32 Slot);

	class UZL_ItemProcComponent* GetProcs() const;

	/** The ability ServerBuyImbuedItem is buying for, read by ServerBuyItem. */
	int32 PendingImbueInput = 0;

	/** Server: gives the item its key's ability. */
	void GrantActiveAbility(FZeroInventoryItem& Item);

	// Helpers
	bool HasEnoughSouls(int32 Cost) const;
	void ChangeSouls(int32 Delta);
	int32 FindItemIndex(const UZero_Item_data* ItemToSearch) const;


public:
	UPROPERTY(BlueprintAssignable)
	FZeroInventoryItemDelegate ItemDelegate;
	
	
	UFUNCTION(BlueprintCallable)
	bool HasSlotToBuy() const;
		
};
