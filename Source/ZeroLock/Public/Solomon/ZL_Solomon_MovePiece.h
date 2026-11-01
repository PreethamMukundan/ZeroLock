// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Solomon_MovePiece.generated.h"

class AZL_Solomon_ChessPiece;
class AZL_Solomon_MovePreview;

/**
 * Solomon's Move Piece, his alt fire (AltFireAbility, right mouse). One piece per press.
 * Hold: picks the piece UZL_Solomon_PieceSelector shows as hovered (his pawn gets the selector in OnAvatarSet, so the
 * hover tint works before the first press), locks it Selected and shows the square it would land on
 * (AZL_Solomon_MovePreview, AZL_Solomon_ChessPiece::GetMoveTarget). Release: moves it there; no valid square = nothing.
 * No piece to pick = nothing happens. LocalPredicted: the aiming client sends piece + aim point as target data
 * (SingleTargetHit: actor = piece, Location = aim); the server checks owner, CanMove and range (+ ServerRangeSlack) and
 * starts the move, which picks the square again with the same rule. Each piece has its own move cooldown; the ability
 * has none.
 */
UCLASS()
class ZEROLOCK_API UZL_Solomon_MovePiece : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Solomon_MovePiece();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "MovePiece")
	float ServerRangeSlack = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "MovePiece")
	TSubclassOf<AZL_Solomon_MovePreview> PreviewClass;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	TWeakObjectPtr<AZL_Solomon_ChessPiece> SelectedPiece;
	UPROPERTY()
	TObjectPtr<AZL_Solomon_MovePreview> Preview;
	FDelegateHandle TargetDataHandle;
	bool bConfirmed = false;

	/** Aiming client: send piece + aim (or run it directly on a listen server host), then end. */
	void ConfirmMove();
	/** Server. */
	void OnServerTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);
	void ExecuteMove(AZL_Solomon_ChessPiece* Piece, const FVector& Aim);
	float GetSelectRange() const;
	float GetAimDistance() const;
};
