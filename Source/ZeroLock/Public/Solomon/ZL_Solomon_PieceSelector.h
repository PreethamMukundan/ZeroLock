// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZL_Solomon_PieceSelector.generated.h"

class AZL_Solomon_ChessPiece;

/**
 * Which of Solomon's pieces his right click (Move Piece) would pick, and the highlight that shows it. Added to his pawn
 * at runtime by UZL_Solomon_MovePiece (Get). On the locally controlled pawn it ticks: the piece FindPieceToMove returns
 * is tinted Hover; while Move Piece holds one (LockSelection) that one is Selected and hovering pauses.
 * FindPieceToMove: his movable pieces (CanMove) within SelectRange of him, picking the one nearest the camera aim within
 * SelectMaxAngle (or SelectRadius of the aim line), aiming at the piece's middle. With bOneMoveAtATime nothing can be
 * picked while one of his pieces is still sliding. A piece riding on Solomon himself (Kingside Knight's king) is always
 * near the aim line in third person, so it's only picked when no other piece is.
 */
UCLASS(ClassGroup = (Solomon), meta = (BlueprintSpawnableComponent))
class ZEROLOCK_API UZL_Solomon_PieceSelector : public UActorComponent
{
	GENERATED_BODY()

public:
	UZL_Solomon_PieceSelector();

	/** The hero's selector, created on first use. */
	static UZL_Solomon_PieceSelector* Get(AActor* Hero);

	/** Local: the piece right click would pick now (needs the owner's player controller for the camera). */
	AZL_Solomon_ChessPiece* FindPieceToMove() const;

	/** Move Piece picked Piece (null = released). */
	void LockSelection(AZL_Solomon_ChessPiece* Piece);

	float GetSelectRange() const { return SelectRange; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Max distance from Solomon to a piece he can move (Deadlock Move Piece AbilityCastRange 50 m). */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float SelectRange = 3500.0f;

	/** A piece within this angle of the crosshair can be picked... */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float SelectMaxAngle = 25.0f;

	/** ...or within this distance of the aim line (cm; Deadlock Radius 3 m). */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float SelectRadius = 210.0f;

	/** Only one piece slides at a time: nothing can be picked while one is moving. */
	UPROPERTY(EditAnywhere, Category = "Selection")
	bool bOneMoveAtATime = true;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AZL_Solomon_ChessPiece> Hovered;
	TWeakObjectPtr<AZL_Solomon_ChessPiece> Locked;

	void SetHovered(AZL_Solomon_ChessPiece* Piece);
};
