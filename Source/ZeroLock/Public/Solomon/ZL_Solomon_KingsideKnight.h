// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseConeTargetAbility.h"
#include "ZL_Solomon_KingsideKnight.generated.h"

class AZL_Solomon_KnightPiece;

/**
 * Solomon's Kingside Knight (Deadlock ability_chessmaster_knightbuff). Cone targeting on allies like Viscous's Cube
 * (UZL_BaseConeTargetAbility): confirm on an ally promotes them to knight, self-cast (double tap, or confirm with nobody
 * in the cone) makes Solomon the king.
 * Server, on confirm: KnightBarrier combat barrier for BuffDuration + BonusStamina stamina at once (king: KingBarrier, no
 * stamina; UBaseCharAbilitySystemComponent::AddCombatBarrier, UZL_Solomon_KnightEffect with Zerolock.Status.Knight /
 * King), and an AZL_Solomon_KnightPiece on them for the same time (replaces his previous one, MaxPieces 1).
 * Move Piece (alt fire) hops the knight or king (a chess-knight L, as a ball) like any piece. With his king out and able to move, pressing this
 * ability again hops him toward the crosshair instead of casting (free; CheckCooldown passes): the aiming client sends
 * the aim as target data, the server re-picks the spot with the piece's own rule.
 */
UCLASS()
class ZEROLOCK_API UZL_Solomon_KingsideKnight : public UZL_BaseConeTargetAbility
{
	GENERATED_BODY()

public:
	UZL_Solomon_KingsideKnight();

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	TSubclassOf<AZL_Solomon_KnightPiece> PieceClass;

	/** Seconds the buff and the piece last (Deadlock BuffDuration 7). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	FScalableFloat BuffDuration = 7.0f;

	/** Ally knight's combat barrier (Deadlock CombatBarrier 80). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	FScalableFloat KnightBarrier = 80.0f;

	/** Solomon as king: double the barrier, no stamina. */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	FScalableFloat KingBarrier = 160.0f;

	/** Stamina an ally knight gets right away, up to their max (Deadlock Stamina 1). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	FScalableFloat BonusStamina = 1.0f;

	/** Speed of the knight / king hop along its L (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	FScalableFloat HopSpeed = 3500.0f;

	/** Aim trace length for the king's re-press move. */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	float KingAimDistance = 3000.0f;

	virtual void ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data) override;

private:
	FDelegateHandle KingMoveDataHandle;

	/** His own knight piece riding on him that can move now, or null. */
	AZL_Solomon_KnightPiece* FindMovableKing(const AActor* Avatar) const;
	void StartKingMove(AZL_Solomon_KnightPiece* King);
	/** Server, remote client's king move. */
	void OnKingMoveData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);
};
