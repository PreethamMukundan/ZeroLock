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

	/** Replace PC's pawn with a fresh HeroClass pawn at the same spot. Without a pawn it only records the selection. */
	void SwapHero(class AZero_BasePlayerController* PC, TSubclassOf<AZeroLockCharacter> HeroClass);

private:
	void AssignTeam(class AZero_BasePlayerState* PS);
	APawn* SpawnPawnDefault(TSubclassOf<AZeroLockCharacter> PawnClass, AController* Controller, AActor* StartSpot);


public:
	void Killed(AController* Killer, AController* Victim);

	UPROPERTY(EditAnywhere)
	float AssistWindow = 5.f;

	
};
