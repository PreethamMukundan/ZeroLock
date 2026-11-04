//Copyright Preetham Mukundan (C) 2026


#include "UI/MVVM/GameStats/ZL_VM_PlayerInfo.h"

#include "UI/MVVM/ZL_VM_Attributes.h"
#include "Zero_BasePlayerState.h"

void UZL_VM_PlayerInfo::SetOwningPlayerState(AZero_BasePlayerState* InPlayerState)
{
	OwningPlayerState = InPlayerState;
}

AZero_BasePlayerState* UZL_VM_PlayerInfo::GetOwningPlayerState() const
{
	return OwningPlayerState.Get();
}

void UZL_VM_PlayerInfo::SetVM_Attributes(UZL_VM_Attributes* newAttVM)
{
	UE_MVVM_SET_PROPERTY_VALUE(VM_Attributes, newAttVM);
}

void UZL_VM_PlayerInfo::SetPlayerName(const FText& NewPlayerName)
{
	UE_MVVM_SET_PROPERTY_VALUE(PlayerName, NewPlayerName);
}

void UZL_VM_PlayerInfo::SetIcon(UTexture2D* NewIcon)
{
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);
}

void UZL_VM_PlayerInfo::SetKills(int32 NewKills)
{
	UE_MVVM_SET_PROPERTY_VALUE(Kills, NewKills);
}

void UZL_VM_PlayerInfo::SetDeaths(int32 NewDeaths)
{
	UE_MVVM_SET_PROPERTY_VALUE(Deaths, NewDeaths);
}

void UZL_VM_PlayerInfo::SetAssists(int32 NewAssists)
{
	UE_MVVM_SET_PROPERTY_VALUE(Assists, NewAssists);
}
