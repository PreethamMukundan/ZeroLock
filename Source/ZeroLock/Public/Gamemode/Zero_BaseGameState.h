//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Zero_BaseGameState.generated.h"
class UZL_VM_GameState;
class UZL_VM_PlayerInfo;
class AZero_BasePlayerState;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamArrayUpdated , AZero_BasePlayerState* , ps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameTimeUpdated , int , gameTime);

enum class ETeamID : uint8;
//class AZero_BasePlayerState;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API AZero_BaseGameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	/** Copied from AZero_BaseGameModeBase::bAllowHeroSwap in InitGameState so clients know whether the hero select (H) may open. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heroes")
	bool bAllowHeroSwap = false;

	/** Copied from AZero_BaseGameModeBase::bPickHeroOnJoin. Clients open the hero select on arrival while they have no pawn. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heroes")
	bool bPickHeroOnJoin = false;

	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 TeamRedScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 TeamBlueScore = 0;

	void AddKill(ETeamID TeamID);


	UPROPERTY(ReplicatedUsing= On_RedTeamArray, BlueprintReadOnly)
	TArray<AZero_BasePlayerState*> TeamRedArray;

	UPROPERTY(ReplicatedUsing = On_BlueTeamArray , BlueprintReadOnly)
	TArray<AZero_BasePlayerState*> TeamBlueArray;

	UPROPERTY(ReplicatedUsing=On_RepGameTime, BlueprintReadOnly )
	int32 myGameTime = 0;

	UFUNCTION()
	void AddPSToTeamArray(AZero_BasePlayerState* psToAdd,ETeamID teamToAddto);

	UFUNCTION()
	void IncrementGameTime();

	UFUNCTION()
	void On_BlueTeamArray();

	UFUNCTION()
	void On_RedTeamArray();


	UPROPERTY(BlueprintAssignable)
	FOnTeamArrayUpdated OnBlueTeamArrayUpdated;
	UPROPERTY(BlueprintAssignable)
	FOnTeamArrayUpdated OnRedTeamArrayUpdated;

	UPROPERTY(BlueprintAssignable)
	FOnGameTimeUpdated OnGameTimeUpdated;

	UFUNCTION()
	void On_RepGameTime();


	UPROPERTY(BlueprintReadOnly, Category = "UI")
	UZL_VM_GameState* GameStateVM;

	UFUNCTION()
	UZL_VM_GameState* GetGameStateVM();

	// Helper to find the VM associated with a specific PlayerState
	UPROPERTY()
	TMap<AZero_BasePlayerState*, UZL_VM_PlayerInfo*> PlayerVMMapping;

	void UpdatePlayerInVM(AZero_BasePlayerState* PS);


	void RefreshTeamLists();
	void ProcessTeamUpdate(const TArray<AZero_BasePlayerState*>& SourceTeamArray);

	/** Helper to get the local player state safely */
	AZero_BasePlayerState* GetLocalPlayerState() const;
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	

	
};
