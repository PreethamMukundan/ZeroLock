//Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_GameState_Bar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/HUD/ZL_PlayerInfoBox.h"


void UZL_GameState_Bar::OnAllyTeamChanged(const TArray<UZL_VM_PlayerInfo*>& NewTeam)
{
	RebuildTeam(AllyTeamList, NewTeam, true);
}

void UZL_GameState_Bar::OnEnemyTeamChanged(const TArray<UZL_VM_PlayerInfo*>& NewTeam)
{
	RebuildTeam(EnemyTeamList, NewTeam, false);
}

void UZL_GameState_Bar::RebuildTeam(UHorizontalBox* List, const TArray<UZL_VM_PlayerInfo*>& NewTeam, bool bAlly)
{
	if (!List)
	{
		return;
	}
	List->ClearChildren();

	TSubclassOf<UUserWidget> CardClass = PlayerIconClass;
	if (!CardClass || !CardClass->IsChildOf(UZL_PlayerInfoBox::StaticClass()))
	{
		CardClass = UZL_PlayerInfoBox::StaticClass();
	}

	for (UZL_VM_PlayerInfo* PlayerVM : NewTeam)
	{
		UZL_PlayerInfoBox* NewWidget = CreateWidget<UZL_PlayerInfoBox>(this, CardClass);
		if (!NewWidget)
		{
			continue;
		}
		NewWidget->SetIsAlly(bAlly);
		// The ally list is left of the timer: the card mirrors so its bar sits on the outer side.
		NewWidget->SetHealthBarMirrored(bAlly);
		NewWidget->SetViewModel(PlayerVM);
		if (UHorizontalBoxSlot* CardSlot = List->AddChildToHorizontalBox(NewWidget))
		{
			CardSlot->SetPadding(FMargin(CardSpacing * 0.5f, 0.f));
			CardSlot->SetVerticalAlignment(VAlign_Top);
		}
	}
}
