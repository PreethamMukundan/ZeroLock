//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Zero_BaseGameModeBase.generated.h"

class AZeroLockCharacter;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API AZero_BaseGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AZero_BaseGameModeBase();

	virtual void BeginPlay() override;
	/** Ends the telemetry match (no win condition yet, so a match lasts as long as the map). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION()
	void UpdateGameTime(AZero_BaseGameState* GS);

	FTimerHandle GameTimerHandle;

	/** Hero selection */
	UPROPERTY(EditDefaultsOnly, Category="Heroes")
	TSubclassOf<AZeroLockCharacter> DefaultHeroClass;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	// Death + Respawn
	void HandlePlayerDeath(AZeroLockCharacter* DeadPawn, AController* DeadController);
	void RespawnPlayer(AController* Controller); // changed to accept Controller (safe)

	/** Whether this game mode allows swapping hero mid-match (H key / swap pad). Off: the hero select won't open and ServerSwapHero is rejected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heroes")
	bool bAllowHeroSwap = false;

	virtual void InitGameState() override;

	/** If true (and bAllowHeroSwap), ServerSwapHero is only accepted while the pawn stands on an AZL_HeroSwapPad. Off: swap anywhere (H key). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heroes")
	bool bHeroSwapRequiresPad = false;

	/**
	 * Players arriving in this mode (login or travel) get no pawn until they pick a hero: the HUD opens the hero select,
	 * the camera sits at a team start, and the first pick (ServerSwapHero, allowed even without bAllowHeroSwap) spawns them.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heroes")
	bool bPickHeroOnJoin = false;

	/** bPickHeroOnJoin and this player has no pawn and no hero picked yet. */
	bool IsWaitingForHeroPick(const AController* Player) const;

	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void GenericPlayerInitialization(AController* C) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Replace PC's pawn with a fresh HeroClass pawn at the same spot. Without a pawn it records the selection, and spawns the player if bPickHeroOnJoin. */
	void SwapHero(class AZero_BasePlayerController* PC, TSubclassOf<AZeroLockCharacter> HeroClass);

private:
	void AssignTeam(class AZero_BasePlayerState* PS);
	APawn* SpawnPawnDefault(TSubclassOf<AZeroLockCharacter> PawnClass, AController* Controller, AActor* StartSpot);


public:
	void Killed(AController* Killer, AController* Victim);

	UPROPERTY(EditAnywhere)
	float AssistWindow = 5.f;

	
};
