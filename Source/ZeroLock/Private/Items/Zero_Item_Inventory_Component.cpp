//Copyright Preetham Mukundan (C) 2026


#include "Items/Zero_Item_Inventory_Component.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "Items/ZL_GA_ActiveItem.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Items/ZL_ItemProcComponent.h"
#include "Items/Zero_Item_data.h"
#include "Net/UnrealNetwork.h"
#include "Telemetry/ZL_TelemetrySubsystem.h"
#include "ZeroLock/ZeroLockCharacter.h"


UZero_Item_Inventory_Component::UZero_Item_Inventory_Component()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UZero_Item_Inventory_Component::BeginPlay()
{
	Super::BeginPlay();
}

void UZero_Item_Inventory_Component::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UZero_Item_Inventory_Component, Items);
}

UAbilitySystemComponent* UZero_Item_Inventory_Component::GetASC() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
}

void UZero_Item_Inventory_Component::OnRep_Items()
{
	ItemDelegate.Broadcast(Items);
}

void UZero_Item_Inventory_Component::ServerBuyItem_Implementation(UZero_Item_data* ItemData,
	UZero_Item_data* ItemUpgradedFrom)
{
	if (!ItemData || HasItem(ItemData) || IsUpgradedAway(ItemData)) return;

	const TArray<UZero_Item_data*> Components = GetOwnedComponents(ItemData);
	if (Components.IsEmpty() && !HasSlotToBuy()) return;

	// An upgrade keeps the key and the cooldown of the active item it replaces.
	int32 ActiveSlot = INDEX_NONE;
	int32 CooldownSlot = INDEX_NONE;
	if (ItemData->IsActiveItem())
	{
		for (const UZero_Item_data* Component : Components)
		{
			const FZeroInventoryItem& Owned = Items[FindItemIndex(Component)];
			if (Owned.ActiveSlot != INDEX_NONE && (ActiveSlot == INDEX_NONE || Owned.ActiveSlot < ActiveSlot))
			{
				ActiveSlot = Owned.ActiveSlot;
				CooldownSlot = Owned.CooldownSlot;
			}
		}
		if (ActiveSlot == INDEX_NONE)
		{
			ActiveSlot = FindFreeActiveSlot();
			CooldownSlot = FindFreeCooldownSlot();
		}
		if (ActiveSlot == INDEX_NONE) return;
	}

	// An imbue upgrade stays on the ability of what it replaces.
	int32 ImbueInput = PendingImbueInput;
	for (const UZero_Item_data* Component : Components)
	{
		ImbueInput = FMath::Max(ImbueInput, GetImbueInput(Component));
	}
	if (ItemData->bImbue && ImbueInput == 0) return;

	int32 Price = ItemData->Cost;
	for (const UZero_Item_data* Component : Components)
	{
		Price -= Component->Cost;
	}
	Price = FMath::Max(Price, 0);
	if (!HasEnoughSouls(Price)) return;

	UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this);
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());

	// An upgrade takes over the slot of what it replaces.
	int32 Slot = Items.Num();
	for (UZero_Item_data* Component : Components)
	{
		const int32 Index = FindItemIndex(Component);
		RemoveItemEffects(Items[Index]);
		Items.RemoveAt(Index);
		Slot = FMath::Min(Slot, Index);
		if (Telemetry)
		{
			Telemetry->RecordItem(Hero, Component, false);
		}
	}

	FZeroInventoryItem NewItem;
	NewItem.ItemData = ItemData;
	NewItem.ActiveSlot = ActiveSlot;
	NewItem.CooldownSlot = CooldownSlot;
	NewItem.ImbueInputID = ItemData->bImbue ? ImbueInput : 0;
	Items.Insert(NewItem, Slot);

	ChangeSouls(-Price);
	ApplyItemEffects(Items[Slot]);
	ItemDelegate.Broadcast(Items);
	if (Telemetry)
	{
		Telemetry->RecordItem(Hero, ItemData, true);
	}
}

void UZero_Item_Inventory_Component::ServerBuyImbuedItem_Implementation(UZero_Item_data* ItemData, int32 InputID)
{
	const bool bHeroAbility = InputID == static_cast<int32>(EGASAbilityInputID::Secondry_Attack)
		|| InputID == static_cast<int32>(EGASAbilityInputID::Ability_1)
		|| InputID == static_cast<int32>(EGASAbilityInputID::Ability_2)
		|| InputID == static_cast<int32>(EGASAbilityInputID::Ultimate);
	if (bHeroAbility)
	{
		TGuardValue<int32> InputGuard(PendingImbueInput, InputID);
		ServerBuyItem_Implementation(ItemData, nullptr);
	}
}

void UZero_Item_Inventory_Component::ServerSellItem_Implementation(UZero_Item_data* ItemData)
{
	const int32 Index = FindItemIndex(ItemData);
	if (Index == INDEX_NONE) return;

	ChangeSouls(FMath::FloorToInt32(ItemData->Cost * ItemData->SellRefundPercent));

	// The next item to use this cooldown tag starts ready.
	const FGameplayTag CooldownTag = ZLActiveItem::GetCooldownTag(Items[Index].CooldownSlot);
	UAbilitySystemComponent* ASC = GetASC();
	if (ASC && CooldownTag.IsValid())
	{
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CooldownTag));
	}

	RemoveItemEffects(Items[Index]);
	Items.RemoveAt(Index);
	ItemDelegate.Broadcast(Items);
	if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
	{
		Telemetry->RecordItem(Cast<AZeroLockCharacter>(GetOwner()), ItemData, false);
	}
}

void UZero_Item_Inventory_Component::ServerMoveActiveItem_Implementation(int32 FromSlot, int32 ToSlot)
{
	if (FromSlot == ToSlot || ToSlot < 0 || ToSlot >= MaxActiveSlots) return;

	const int32 MovedIndex = FindActiveItemIndex(FromSlot);
	if (MovedIndex == INDEX_NONE) return;

	const int32 DisplacedIndex = FindActiveItemIndex(ToSlot);
	SetActiveSlot(Items[MovedIndex], ToSlot);
	if (DisplacedIndex != INDEX_NONE)
	{
		SetActiveSlot(Items[DisplacedIndex], FromSlot);
	}
	ItemDelegate.Broadcast(Items);
}

void UZero_Item_Inventory_Component::SetActiveSlot(FZeroInventoryItem& Item, int32 Slot)
{
	Item.ActiveSlot = Slot;

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	for (const FGameplayAbilitySpecHandle& Handle : Item.AbilitySpecHandles)
	{
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		if (Spec && Spec->InputID != static_cast<int32>(EGASAbilityInputID::None))
		{
			Spec->InputID = static_cast<int32>(EGASAbilityInputID::Item_1) + Slot;
			ASC->MarkAbilitySpecDirty(*Spec);
		}
	}
}

UZL_ItemProcComponent* UZero_Item_Inventory_Component::GetProcs() const
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
	return Hero ? Hero->GetItemProcs() : nullptr;
}

void UZero_Item_Inventory_Component::ReapplyItemEffects()
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	if (UZL_ItemProcComponent* Procs = GetProcs())
	{
		Procs->ResetAfterRespawn();
	}

	for (FZeroInventoryItem& Item : Items)
	{
		ApplyStatEffects(Item);

		for (const FGameplayAbilitySpecHandle& Handle : Item.AbilitySpecHandles)
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			const UBaseGameplayAbility* Ability = Spec ? Cast<UBaseGameplayAbility>(Spec->Ability) : nullptr;
			if (Ability && Ability->TargetStyle == EGASTargetConfirmationStyle::Passive)
			{
				ASC->TryActivateAbility(Handle);
			}
		}
	}
}

void UZero_Item_Inventory_Component::ApplyStatEffects(FZeroInventoryItem& Item)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Item.ItemData) return;

	// A respawn already removed the old effects, so these handles are stale.
	Item.StatEffectHandles.Reset();

	if (Item.ItemData->PassiveEffects)
	{
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Item.ItemData->PassiveEffects, 1, ASC->MakeEffectContext());
		if (SpecHandle.IsValid())
		{
			Item.StatEffectHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data));
		}
	}

	if (!Item.ItemData->Stats.IsEmpty())
	{
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UZL_GE_ItemStats::StaticClass(), 1, ASC->MakeEffectContext());
		if (SpecHandle.IsValid())
		{
			UZL_GE_ItemStats::SetMagnitudes(*SpecHandle.Data, Item.ItemData->Stats);
			Item.StatEffectHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data));
		}
	}
}

void UZero_Item_Inventory_Component::ApplyItemEffects(FZeroInventoryItem& Item)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Item.ItemData) return;

	ApplyStatEffects(Item);

	for (const TSubclassOf<UBaseGameplayAbility>& Ability : Item.ItemData->GrantedAbilities)
	{
		if (Ability)
		{
			const FGameplayAbilitySpec GrantedSpec(Ability, 1, static_cast<int32>(EGASAbilityInputID::None), this);
			Item.AbilitySpecHandles.Add(ASC->GiveAbility(GrantedSpec));
		}
	}

	GrantActiveAbility(Item);

	if (UZL_ItemProcComponent* Procs = GetProcs())
	{
		Procs->AddItem(Item.ItemData);
	}
}

void UZero_Item_Inventory_Component::GrantActiveAbility(FZeroInventoryItem& Item)
{
	UAbilitySystemComponent* ASC = GetASC();
	const TSubclassOf<UBaseGameplayAbility> AbilityClass = Item.ItemData ? ZLActiveItem::GetAbilityClass(Item.ItemData->Active.Target) : nullptr;
	if (!ASC || !AbilityClass || Item.ActiveSlot == INDEX_NONE)
	{
		return;
	}

	// The item as source object is how the ability finds its data, and the spec's tag its cooldown, on the client too.
	const int32 InputID = static_cast<int32>(EGASAbilityInputID::Item_1) + Item.ActiveSlot;
	FGameplayAbilitySpec Spec(AbilityClass, 1, InputID, Item.ItemData);
	Spec.GetDynamicSpecSourceTags().AddTag(ZLActiveItem::GetCooldownTag(Item.CooldownSlot));
	Item.AbilitySpecHandles.Add(ASC->GiveAbility(Spec));
}

void UZero_Item_Inventory_Component::RemoveItemEffects(FZeroInventoryItem& Item)
{
	if (!IsValid(Item.ItemData)) return;
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	if (UZL_ItemProcComponent* Procs = GetProcs())
	{
		Procs->RemoveItem(Item.ItemData);
	}

	for (const FActiveGameplayEffectHandle& Handle : Item.StatEffectHandles)
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}
	Item.StatEffectHandles.Empty();

	for (const FGameplayAbilitySpecHandle& Handle : Item.AbilitySpecHandles)
	{
		ASC->ClearAbility(Handle);
	}

	Item.AbilitySpecHandles.Empty();
	
}

bool UZero_Item_Inventory_Component::HasEnoughSouls(int32 Cost) const
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
	return Hero && Hero->GetMyAttributeSet()->GetSoul() >= Cost;
}

void UZero_Item_Inventory_Component::ChangeSouls(int32 Delta)
{
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		const float Souls = ASC->GetNumericAttributeBase(UBaseCharAttributeSet::GetSoulAttribute());
		ASC->SetNumericAttributeBase(UBaseCharAttributeSet::GetSoulAttribute(), FMath::Max(Souls + Delta, 0.0f));
	}
}

int32 UZero_Item_Inventory_Component::FindItemIndex(const UZero_Item_data* ItemToSearch) const
{
	return Items.IndexOfByPredicate([ItemToSearch](const FZeroInventoryItem& Item)
	{
		return Item.ItemData == ItemToSearch;
	});
}

bool UZero_Item_Inventory_Component::NeedsImbueChoice(const UZero_Item_data* ItemData) const
{
	if (!ItemData || !ItemData->bImbue)
	{
		return false;
	}
	for (const UZero_Item_data* Component : GetOwnedComponents(ItemData))
	{
		if (GetImbueInput(Component) != 0)
		{
			return false;
		}
	}
	return true;
}

int32 UZero_Item_Inventory_Component::GetImbueInput(const UZero_Item_data* ItemData) const
{
	const int32 Index = FindItemIndex(ItemData);
	return Index != INDEX_NONE ? Items[Index].ImbueInputID : 0;
}

float UZero_Item_Inventory_Component::GetImbueBonus(EZL_ItemStat Stat, int32 InputID) const
{
	float Total = 0.0f;
	for (const FZeroInventoryItem& Item : Items)
	{
		if (Item.ImbueInputID == InputID && InputID != 0 && Item.ItemData)
		{
			for (const FZL_ItemStatValue& Value : Item.ItemData->ImbueStats)
			{
				if (Value.Stat == Stat)
				{
					Total += Value.Value;
				}
			}
		}
	}
	return Total;
}

bool UZero_Item_Inventory_Component::HasItem(const UZero_Item_data* ItemData) const
{
	return ItemData && FindItemIndex(ItemData) != INDEX_NONE;
}

const UZero_Item_data* UZero_Item_Inventory_Component::GetActiveItem(int32 Slot) const
{
	const int32 Index = FindActiveItemIndex(Slot);
	return Index != INDEX_NONE ? Items[Index].ItemData : nullptr;
}

int32 UZero_Item_Inventory_Component::GetActiveCooldownSlot(int32 Slot) const
{
	const int32 Index = FindActiveItemIndex(Slot);
	return Index != INDEX_NONE ? Items[Index].CooldownSlot : INDEX_NONE;
}

int32 UZero_Item_Inventory_Component::FindActiveItemIndex(int32 Slot) const
{
	if (Slot == INDEX_NONE) return INDEX_NONE;

	return Items.IndexOfByPredicate([Slot](const FZeroInventoryItem& Item)
	{
		return Item.ActiveSlot == Slot;
	});
}

int32 UZero_Item_Inventory_Component::FindFreeActiveSlot() const
{
	for (int32 Slot = 0; Slot < MaxActiveSlots; Slot++)
	{
		if (!GetActiveItem(Slot))
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

int32 UZero_Item_Inventory_Component::FindFreeCooldownSlot() const
{
	for (int32 Slot = 0; Slot < MaxActiveSlots; Slot++)
	{
		const bool bTaken = Items.ContainsByPredicate([Slot](const FZeroInventoryItem& Item)
		{
			return Item.CooldownSlot == Slot;
		});
		if (!bTaken)
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

TArray<UZero_Item_data*> UZero_Item_Inventory_Component::GetOwnedComponents(const UZero_Item_data* ItemData) const
{
	TArray<UZero_Item_data*> Components;
	for (const FZeroInventoryItem& Item : Items)
	{
		if (Item.ItemData && Item.ItemData->NextItemsToUpgrade.Contains(ItemData))
		{
			Components.Add(Item.ItemData);
		}
	}
	return Components;
}

bool UZero_Item_Inventory_Component::IsUpgradedAway(const UZero_Item_data* ItemData) const
{
	if (!ItemData) return false;

	for (const UZero_Item_data* Next : ItemData->NextItemsToUpgrade)
	{
		if (HasItem(Next) || IsUpgradedAway(Next))
		{
			return true;
		}
	}
	return false;
}

bool UZero_Item_Inventory_Component::HasSlotToBuy() const
{
	return Items.Num() < MaxSlots;
}
