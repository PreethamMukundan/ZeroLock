// Copyright Preetham Mukundan (C) 2026


#include "UI/MVVM/Input/ZL_VM_KeyBindingList.h"

#include "Input/KeybindManagerSubsystem.h"
#include "UI/MVVM/Input/ZL_VM_KeyBindingRow.h"
#include "ZeroLock/ZeroLock.h"

void UZL_VM_KeyBindingList::InitializeWithPlayer(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer) return;

	SubsystemRef = LocalPlayer->GetSubsystem<UKeybindManagerSubsystem>();
	if (SubsystemRef)
	{

		SubsystemRef->OnKeybindsUpdated.AddDynamic(this, &UZL_VM_KeyBindingList::HandleSubsystemBindingsUpdated);
		
		HandleSubsystemBindingsUpdated();
	}
}

void UZL_VM_KeyBindingList::RequestKeyRemap(UZL_VM_KeyBindingRow* RowVM, FKey NewKey)
{
	if (SubsystemRef && RowVM && !RowVM->MappingName.IsNone())
	{
		SubsystemRef->RemapKey(RowVM->MappingName, NewKey);
	}
}

void UZL_VM_KeyBindingList::HandleSubsystemBindingsUpdated()
{
	if (!SubsystemRef) return;

	KeybindRows.Empty();

	TArray<FKeybindData> CurrentData = SubsystemRef->GetCurrentKeybinds();
	
	for (const FKeybindData& Data : CurrentData)
	{
		if (!Data.CurrentKey.IsValid()) continue;
		UZL_VM_KeyBindingRow* NewRow = NewObject<UZL_VM_KeyBindingRow>(this);
		NewRow->TargetAction = Data.Action;
		NewRow->SetMappingName(Data.MappingName);
		NewRow->SetActionName(Data.ActionName);
		NewRow->SetCurrentKey(Data.CurrentKey);
		NewRow->DisplayCategory = Data.DisplayCategory;
		NewRow->SetSubsystemRef(SubsystemRef);
        
		KeybindRows.Add(NewRow);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KeybindRows);
}
