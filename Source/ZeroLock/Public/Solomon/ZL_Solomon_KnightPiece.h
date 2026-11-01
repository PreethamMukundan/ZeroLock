// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Solomon/ZL_Solomon_ChessPiece.h"
#include "ZL_Solomon_KnightPiece.generated.h"

/**
 * Kingside Knight's piece: rides on the promoted player (an ally = knight, Solomon himself = king), at their feet, so
 * Move Piece (UZL_Solomon_MovePiece) can pick it by aiming at them, and the king's re-press of Kingside Knight can find
 * it. Spawned by the server (UZL_Solomon_KingsideKnight) for the buff's duration; gone early if the rider or Solomon
 * dies. Doesn't take part in piece spacing.
 *
 * A move is a chess-knight hop of the rider: the end is anywhere in range (KnightMoveDistance, KingMoveDistance for the
 * king; same GetMoveTarget rule as the other pieces, on the fine grid), reached by an L: a long leg then a short
 * sideways step (2 : 1, the side picked at random by the server), at MoveSpeed. Server: two Zerolock.Movement.PullTo
 * victim events (one per leg, the second on a timer) and a zero Zerolock.Movement.Forced at the end so the rider
 * doesn't keep the pull's speed. The rider is Untouchable for the whole move (UZL_Solomon_KnightEffect) and shows as
 * a ball (their mesh hidden, BallMesh shown) on every machine while IsMoving (replicated FlightEndTime). MoveCooldown
 * is per piece as usual (Deadlock 10 s > the 7 s buff: one move per cast).
 */
UCLASS()
class ZEROLOCK_API AZL_Solomon_KnightPiece : public AZL_Solomon_ChessPiece
{
	GENERATED_BODY()

public:
	AZL_Solomon_KnightPiece();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, right after spawning: rides on Rider for Lifetime seconds, hopping at HopSpeed (cm/s; <= 0 keeps MoveSpeed). */
	void InitPromotion(AZeroLockCharacter* Rider, float Lifetime, float HopSpeed = 0.0f);

	/** The rider is Solomon himself. */
	bool IsKing() const;

	virtual AZeroLockCharacter* GetCarriedHero() const override { return Rider; }
	virtual bool BlocksBoard() const override { return false; }
	virtual FVector GetBoardLocation() const override;
	virtual bool IsMoving() const override;
	virtual bool ServerStartMove(const FVector& Target) override;
	virtual void GetPreviewPath(const FVector& End, TArray<FVector>& OutPath) const override;

protected:
	/** Move range of an ally knight (cm; Deadlock MoveDistance 13 m). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	float KnightMoveDistance = 910.0f;

	/** Move range of Solomon as king (cm; Deadlock SelfMoveDistance 7 m). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	float KingMoveDistance = 490.0f;

	/** The corner of the L is this much above the straight line (cm), so the pull doesn't scrape the floor. */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	float CornerLift = 40.0f;

	/** Shortest time for one leg (s). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight", meta = (ClampMin = 0.02))
	float MinLegDuration = 0.08f;

	/** Untouchable this long past the end of the move. */
	UPROPERTY(EditDefaultsOnly, Category = "Knight")
	float InvulnerableSlack = 0.15f;

	/** Placeholder marker above the rider's head (cm above the capsule top). */
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Look")
	float MarkerHeight = 40.0f;

	/** The ball the rider turns into while moving (engine Sphere, sized to the capsule in OnRep_Rider). */
	UPROPERTY(VisibleAnywhere, Category = "Knight|Look")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	/** Ball diameter / capsule diameter. */
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Look")
	float BallScale = 1.3f;

	UPROPERTY(ReplicatedUsing = OnRep_Rider)
	TObjectPtr<AZeroLockCharacter> Rider;

	/** Server clock time the current move ends. */
	UPROPERTY(Replicated)
	float FlightEndTime = 0.0f;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_Rider();

private:
	/** Capsule center for a feet location (PullTo moves the actor, whose origin is the capsule center). */
	FVector FeetToCenter(const FVector& Feet) const;
	FVector GetRiderFeet() const;
	/** Corner of the knight's L from Start to End: 2 along, 1 to Side (+1 / -1). */
	FVector GetCorner(const FVector& Start, const FVector& End, float Side) const;
	/** Rider's mesh hidden and the ball shown, or back. */
	void SetBallForm(bool bBall);
	/** Server timers: second leg, then the stop. */
	void StartSecondLeg();
	void StopRider();

	bool bBallForm = false;
	FVector SecondLegTarget = FVector::ZeroVector;
	float SecondLegDuration = 0.0f;
	FTimerHandle LegTimer;
	FTimerHandle StopTimer;

	UFUNCTION()
	void OnRiderDestroyed(AActor* DestroyedActor);
	void OnRiderPreDeath(AZeroLockCharacter* DeadHero);
	FDelegateHandle RiderDeathHandle;
};
