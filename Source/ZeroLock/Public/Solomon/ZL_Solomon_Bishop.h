// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Solomon/ZL_Solomon_ChessPiece.h"
#include "ZL_Solomon_Bishop.generated.h"

class AZL_Solomon_BishopSquare;
class UGameplayEffect;

/**
 * Solomon's bishop (Setup Bishop, UZL_Solomon_SetupBishop). Stays where it's placed until recalled or Solomon dies.
 * Moved with Move Piece (Deadlock: 12 m at 1500 u/s, 3 s per-piece cooldown). While moving (server) every enemy within
 * PassRadius of its path takes PassDamage spirit damage once per move and a fading slow: SlowPercent at first, easing to
 * nothing over SlowDuration (UZL_GE_MoveSlow re-applied every SlowTickInterval). When its move cooldown runs out
 * (MoveCooldown after landing, the moment it can move again) it lays squares on its diagonals: the board squares
 * (SquareSize), TileSteps out on each of the four (8 by default) plus one under itself (bSquareUnderPiece), each dropped
 * onto the floor (no floor = no square). They stay until the bishop moves again or goes away, or an enemy steps on one
 * (AZL_Solomon_BishopSquare: SquareDamage, then it's gone).
 */
UCLASS()
class ZEROLOCK_API AZL_Solomon_Bishop : public AZL_Solomon_ChessPiece
{
	GENERATED_BODY()

public:
	AZL_Solomon_Bishop();

protected:
	/** Spirit damage to each enemy the moving bishop passes (Deadlock ImpactDamage 70). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Move")
	float PassDamage = 70.0f;

	/** How close to the bishop's path an enemy gets hit (cm; Deadlock ImpactRadius 2.5 m). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Move")
	float PassRadius = 175.0f;

	/** Starting slow (%), fading to 0 over SlowDuration (Deadlock FadingSlowPercent 30, SlowDuration 3). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Move", meta = (ClampMin = 0, ClampMax = 90))
	float SlowPercent = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Move")
	float SlowDuration = 3.0f;

	/** How often the fading slow steps down (s). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Move")
	float SlowTickInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Move")
	TSubclassOf<UGameplayEffect> SlowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Squares")
	TSubclassOf<AZL_Solomon_BishopSquare> SquareClass;

	/** Also a square right under the bishop. */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Squares")
	bool bSquareUnderPiece = true;

	/** Squares on each diagonal (Deadlock TileSteps 2). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Squares", meta = (ClampMin = 1))
	int32 TileSteps = 2;

	/** Spirit damage to the enemy that steps on a square (Deadlock Damage 25). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Squares")
	float SquareDamage = 25.0f;

	/** How high above the floor a square catches heroes (cm; Deadlock GroundAuraHeight 4 m). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Squares")
	float SquareHeight = 280.0f;

	/** Also lay squares after placement, once MoveCooldown has passed (Deadlock only does it after a move). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop|Squares")
	bool bSquaresOnPlace = false;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnMoveStarted() override;
	virtual void OnMoveStep(const FVector& From, const FVector& To) override;
	virtual void OnMoveFinished() override;

private:
	struct FFadingSlow
	{
		FActiveGameplayEffectHandle Handle;
		float StartTime = 0.0f;
	};

	/** Enemies hit during the current move. */
	TSet<TWeakObjectPtr<AZeroLockCharacter>> HitThisMove;
	TMap<TWeakObjectPtr<AZeroLockCharacter>, FFadingSlow> FadingSlows;
	TArray<TWeakObjectPtr<AZL_Solomon_BishopSquare>> Squares;

	FTimerHandle SquareTimer;
	FTimerHandle SlowTimer;

	void SpawnSquares();
	void ClearSquares();

	void HitVillan(AZeroLockCharacter* Villan);
	/** Re-applies each fading slow at its current strength; drops the finished ones. */
	void TickSlows();
	FActiveGameplayEffectHandle ApplySlow(AZeroLockCharacter* Villan, float Multiplier);
	static void RemoveSlow(AZeroLockCharacter* Villan, const FActiveGameplayEffectHandle& Handle);
};
