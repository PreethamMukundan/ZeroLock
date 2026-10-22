//Copyright Preetham Mukundan (C) 2026


#include "Gamemode/Zero_BaseGameState.h"

#include "Zero_BasePlayerState.h"
#include "Net/UnrealNetwork.h"
#include "UI/MVVM/GameStats/ZL_VM_GameState.h"
#include "UI/MVVM/GameStats/ZL_VM_PlayerInfo.h"
#include "ZeroLock/ZeroLock.h"

void AZero_BaseGameState::AddKill(ETeamID TeamID)
{
	if (TeamID == ETeamID::TeamRed) TeamRedScore++;
	else if (TeamID == ETeamID::TeamBlue) TeamBlueScore++;
}

void AZero_BaseGameState::AddPSToTeamArray(AZero_BasePlayerState* psToAdd, ETeamID teamToAddto)
{
	if (HasAuthority())
	{
		if (teamToAddto == ETeamID::TeamRed)
		{
			TeamRedArray.Add(psToAdd);
			if (OnRedTeamArrayUpdated.IsBound())
			{
				//ZLOG_COLOR_TIME("SERVER_TEAM_RED_ASSIGN",FColor::Red,30);
				OnRedTeamArrayUpdated.Broadcast(psToAdd);
			}
			
		}
		else if (teamToAddto == ETeamID::TeamBlue)
		{
				//ZLOG_COLOR_TIME("SERVER_TEAM_RED_ASSIGN",FColor::Blue,30);
			TeamBlueArray.Add(psToAdd);
			if (OnBlueTeamArrayUpdated.IsBound())
			{
				OnBlueTeamArrayUpdated.Broadcast(psToAdd);
			}
		}
		RefreshTeamLists();
	}
}

void AZero_BaseGameState::IncrementGameTime()
{
	if (HasAuthority())
	{
		myGameTime++;
		
		if (OnGameTimeUpdated.IsBound())
		{
			OnGameTimeUpdated.Broadcast(myGameTime);
		}
		if (GetGameStateVM())
		{
			if (GameStateVM)
			{
				int32 Minutes = myGameTime / 60;
				int32 Seconds = myGameTime % 60;
				GameStateVM->SetFormattedGameTime(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
			}
		}
	}
}

void AZero_BaseGameState::On_BlueTeamArray()
{
	if (TeamBlueArray.IsEmpty()) return;
	if (OnBlueTeamArrayUpdated.IsBound())
	{
		OnBlueTeamArrayUpdated.Broadcast(TeamBlueArray.Last());
	}
	RefreshTeamLists();
}

void AZero_BaseGameState::On_RedTeamArray()
{
	if (TeamRedArray.IsEmpty()) return;
	if (OnRedTeamArrayUpdated.IsBound())
	{
		OnRedTeamArrayUpdated.Broadcast(TeamRedArray.Last());
	}
	RefreshTeamLists();
}

void AZero_BaseGameState::On_RepGameTime()
{
	if (OnGameTimeUpdated.IsBound())
	{
		OnGameTimeUpdated.Broadcast(myGameTime);
	}
	if (GetGameStateVM())
	{
		if (GameStateVM)
		{
			int32 Minutes = myGameTime / 60;
			int32 Seconds = myGameTime % 60;
			GameStateVM->SetFormattedGameTime(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
		}
	}
}

UZL_VM_GameState* AZero_BaseGameState::GetGameStateVM()
{
	if (!GameStateVM)
	{
		GameStateVM=NewObject<UZL_VM_GameState>(this);
		if (!GameStateVM) return nullptr;
	}
	return GameStateVM;

	
}

void AZero_BaseGameState::UpdatePlayerInVM(AZero_BasePlayerState* PS)
{
	if (PlayerVMMapping.Contains(PS))
	{
		UZL_VM_PlayerInfo* VM = PlayerVMMapping[PS];
		VM->SetKills( PS->Kills);
		VM->SetDeaths(PS->Deaths);
		VM->SetAssists(PS->Assists);
		
	}
	else
	{
		RefreshTeamLists();
	}

	
}

void AZero_BaseGameState::RefreshTeamLists()
{
	if (!GameStateVM) return;
	
	GameStateVM->EmptyAllyTeam();
	GameStateVM->EmptyEnemyTeam();

	// Process both replicated arrays
	ProcessTeamUpdate(TeamRedArray);
	ProcessTeamUpdate(TeamBlueArray);
}

void AZero_BaseGameState::ProcessTeamUpdate(const TArray<AZero_BasePlayerState*>& SourceTeamArray)
{
	AZero_BasePlayerState* LocalPS = GetLocalPlayerState();
	if (!LocalPS || LocalPS->TeamID == ETeamID::TeamNull) return;

	for (AZero_BasePlayerState* PS : SourceTeamArray)
	{
		if (!PS) continue;
		
		if (!PlayerVMMapping.Contains(PS))
		{
			UZL_VM_PlayerInfo* NewVM = NewObject<UZL_VM_PlayerInfo>(this);
			PS->SetPlayerIconVM(NewVM);
			if (PS->CurrentVM)
			{
				NewVM->SetVM_Attributes(PS->CurrentVM);
			}
			PlayerVMMapping.Add(PS, NewVM);
		}
		
		UZL_VM_PlayerInfo* VM = PlayerVMMapping[PS];
		VM->PlayerName = FText::FromString(PS->GetPlayerName());
		VM->Icon = PS->PlayerImage;
		VM->Kills = PS->Kills;
		VM->Deaths = PS->Deaths;
		
		if (PS->TeamID == LocalPS->TeamID)
		{
			GameStateVM->AddAllyTeam(VM);
		}
		else
		{
			GameStateVM->AddEnemyTeam(VM);
		}
	}
}

AZero_BasePlayerState* AZero_BaseGameState::GetLocalPlayerState() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? PC->GetPlayerState<AZero_BasePlayerState>() : nullptr;
}

void AZero_BaseGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZero_BaseGameState, bAllowHeroSwap);
	DOREPLIFETIME(AZero_BaseGameState, bPickHeroOnJoin);
	DOREPLIFETIME(AZero_BaseGameState, TelemetryMatchId);
	DOREPLIFETIME(AZero_BaseGameState, TeamRedScore);
	DOREPLIFETIME(AZero_BaseGameState, TeamBlueScore);
	DOREPLIFETIME(AZero_BaseGameState, TeamRedArray);
	DOREPLIFETIME(AZero_BaseGameState, TeamBlueArray);
	DOREPLIFETIME(AZero_BaseGameState, myGameTime);
}
