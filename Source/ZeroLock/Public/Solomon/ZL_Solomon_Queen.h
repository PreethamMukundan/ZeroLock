// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Solomon/ZL_Solomon_ChessPiece.h"
#include "ZL_Solomon_Queen.generated.h"

class AZL_Solomon_QueenTile;
class UGameplayEffect;

/**
 * Solomon's queen (Develop Queen, UZL_Solomon_DevelopQueen). The server spawns it DropHeight above the target and it
 * crashes down over DropDuration (a move along a straight drop, StartMoveAlong). Every landing (the drop and each leap)
 * is a strike (server): Damage spirit + slow to enemies within CrashRadius (WorldStatic line of sight), then after
 * InitialTileDelay squares spread out along the queen's 8 lines (straight and diagonal), one square (SquareSize) every
 * TimeBetweenTiles, TileSteps out; a line stops at a wall or where there's no floor. Each enemy standing in a square
 * takes Damage again + StunEffect for StunDuration + slow, once per strike.
 * Moved with Move Piece like any piece, but leaps in an arc (LeapHeight; MoveDistance 16 m, MoveCooldown 5); MovesLeft
 * moves, then it can't move and goes away LingerAfterLastStrike after its last strike. Also gone when recalled (the
 * ability's re-press), when Solomon gets ExpireDistance away, or when he dies.
 */
UCLASS()
class ZEROLOCK_API AZL_Solomon_Queen : public AZL_Solomon_ChessPiece
{
	GENERATED_BODY()

public:
	AZL_Solomon_Queen();

	virtual void Tick(float DeltaSeconds) override;

	/** Server, between SpawnActorDeferred and FinishSpawning. Land = floor point the queen crashes onto. */
	void InitQueen(const FVector& InLand, float InDamage, float InStunDuration, float InSlowPercent, float InSlowDuration, int32 InTileSteps,
		int32 InMoves, TSubclassOf<UGameplayEffect> InStunEffect);

	float GetDropHeight() const { return DropHeight; }
	float GetCrashRadius() const { return CrashRadius; }

protected:
	/** How high above the target it appears (cm; Deadlock Height 10 m). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Drop")
	float DropHeight = 700.0f;

	/** Fall time = the cast delay (Deadlock AbilityCastDelay 0.75). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Drop")
	float DropDuration = 0.75f;

	/** Crash damage radius around the landing (cm; Deadlock trigger radius 120 u). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Strike")
	float CrashRadius = 213.0f;

	/** How high above the floor the crash and the squares catch heroes (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Strike")
	float StrikeHeight = 280.0f;

	/** Landing to the first squares (Deadlock InitialTileDelay 1). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Strike")
	float InitialTileDelay = 1.0f;

	/** Between square steps (Deadlock TimeBetweenTiles 0.15). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Strike")
	float TimeBetweenTiles = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Queen|Strike")
	TSubclassOf<AZL_Solomon_QueenTile> TileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Queen|Strike")
	TSubclassOf<UGameplayEffect> SlowEffect;

	/** Default stun if the ability passes none. */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Strike")
	TSubclassOf<UGameplayEffect> StunEffect;

	/** Peak of the leap arc above the higher end (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen|Move")
	float LeapHeight = 280.0f;

	/** Gone this long after the strike of its last move. */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	float LingerAfterLastStrike = 1.0f;

	/** Gone when Solomon is farther than this (cm; Deadlock ExpireDistance 80 m). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	float ExpireDistance = 5600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	bool bDrawDebug = false;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void BuildMovePath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const override;
	virtual void OnMoveStarted() override;
	virtual void OnMoveFinished() override;

private:
	struct FQueenSlow
	{
		FActiveGameplayEffectHandle Handle;
		FTimerHandle Timer;
	};

	/** Set by InitQueen (server). */
	FVector Land = FVector::ZeroVector;
	float Damage = 120.0f;
	float StunDuration = 1.25f;
	float SlowPercent = 15.0f;
	float SlowDuration = 1.5f;
	int32 TileSteps = 3;
	int32 MovesLeft = 3;

	bool bDropping = false;

	/** Current strike. */
	FVector StrikeOrigin = FVector::ZeroVector;
	int32 StrikeStep = 0;
	bool bLineAlive[8] = {};
	FVector LineFloor[8];
	TSet<TWeakObjectPtr<AZeroLockCharacter>> HitByTiles;
	FTimerHandle StrikeTimer;
	FTimerHandle ExpireTimer;

	TMap<TWeakObjectPtr<AZeroLockCharacter>, FQueenSlow> Slows;

	void Strike();
	void StepTiles();
	void StrikeFinished();
	void HitVillan(AZeroLockCharacter* Villan, bool bStun);
	void ApplySlow(AZeroLockCharacter* Villan);
	void RemoveSlow(TWeakObjectPtr<AZeroLockCharacter> Villan);
	void Expire();
};
