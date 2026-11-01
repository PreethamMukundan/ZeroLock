// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Solomon_ChessPiece.generated.h"

class AZeroLockCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** One move of a chess piece: the server sets it, every machine slides the piece along it on the server's clock. */
USTRUCT()
struct FZL_ChessPieceMove
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Start = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 End = FVector::ZeroVector;

	/** Ground path from Start to End (both included), evenly spaced along the straight top-down line; the piece follows a
	 * Catmull-Rom curve through it (AZL_Solomon_ChessPiece::BuildGroundPath / EvaluatePath). */
	UPROPERTY()
	TArray<FVector_NetQuantize10> Path;

	/** AGameStateBase::GetServerWorldTimeSeconds when the move started. */
	UPROPERTY()
	float StartTime = 0.0f;

	UPROPERTY()
	float Duration = 0.0f;

	/** Bumped per move so a repeat move to the same spot still replicates. */
	UPROPERTY()
	uint8 MoveId = 0;
};

/** How a piece is tinted on its owner's screen (UZL_Solomon_PieceSelector drives it). */
UENUM()
enum class EZL_PieceHighlight : uint8
{
	None,
	/** Right click would pick this one. */
	Hover,
	/** Picked: right click is held. */
	Selected
};

/**
 * Base for Solomon's pieces (bishop, queen, ...). Spawned by the server, owned and instigated by Solomon.
 *
 * The board: one world-aligned grid of SquareSize squares (centres at multiples of SquareSize) shared by all his pieces,
 * so every piece, bishop square and queen tile lines up. With bSnapToBoard placement (FindPlacementSpot) and moves end on
 * a board square; without it they snap to a fine world grid (SnapSize, 0 = off). Always on standable floor (normal within MaxFloorAngle).
 * Two of his pieces can't be closer than MinPieceSpacing (Chebyshev, so their squares never overlap; a moving piece
 * counts at its destination). SquareSize is the bishop's diagonal square size.
 *
 * Moved only by Move Piece (UZL_Solomon_MovePiece, his alt fire) through ServerStartMove. GetMoveTarget: range is a
 * sphere of MoveDistance around the piece; the probe is the aim point if inside it, else the sphere's edge toward the aim.
 * With bSnapToBoard the move ends on a board square: the square nearest the probe with standable floor (150 above to
 * 600 below), room to stand (HasStandingRoom: not in or against a wall), free and in range wins. Without it: back from
 * the probe toward the piece on the fine SnapSize grid until such a spot is found. The way there may cross walls; the end can't be in
 * one. Aiming at the piece itself is allowed (it moves in place). The move follows the ground (BuildGroundPath: straight
 * seen from above, height from the floor under each sample, hops over gaps and tall walls) at MoveSpeed along the path
 * (at least MinMoveDuration). It replicates as one FZL_ChessPieceMove with the server's path; every machine follows
 * the curve through it (EvaluatePath) on the server clock, so there's no movement replication. Each piece has its own cooldown: MoveCooldown
 * after the move ends. Subclasses react with OnMoveStarted / OnMoveStep (server, the segment covered this tick) /
 * OnMoveFinished (server). The piece goes away with Solomon (destroyed or dead).
 */
UCLASS(Abstract)
class ZEROLOCK_API AZL_Solomon_ChessPiece : public AActor
{
	GENERATED_BODY()

public:
	AZL_Solomon_ChessPiece();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	AZeroLockCharacter* GetHero() const;

	/** Every live piece of this hero (optionally of one class). */
	static TArray<AZL_Solomon_ChessPiece*> GetPiecesOf(const AActor* Hero, TSubclassOf<AZL_Solomon_ChessPiece> PieceClass = nullptr);

	// --- Board ---
	/** X/Y snapped to a SnapSize grid (0 = unchanged). With SquareSize it's the centre of the board square under Location. */
	static FVector SnapToGrid(const FVector& Location, float InSnapSize);
	/** Call on the class default object. Where a piece of this class placed at Point (aim hit) would stand: with bSnapToBoard
	 * the board square under Point, or if that one fails, the neighbouring square whose centre is nearest Point within one
	 * square (aiming at a wall or ledge still finds the square next to it); without it Point on the fine SnapSize grid. Needs standable
	 * floor, room to stand and (bCheckSpacing) IsSpotFree against Hero's pieces. False = no spot (OutFloor = best guess, for the preview). */
	bool FindPlacementSpot(const AActor* Hero, const FVector& Point, bool bCheckSpacing, FVector& OutFloor) const;
	/** False if one of Hero's pieces (except Ignore) is closer than MinSpacing (Chebyshev, same floor: < 200 cm apart in height). */
	static bool IsSpotFree(const AActor* Hero, const FVector& Location, float MinSpacing, const AZL_Solomon_ChessPiece* Ignore = nullptr);
	/** Floor under Point (WorldStatic trace from SearchUp above to SearchDown below) whose normal is within MaxFloorAngle of up. */
	static bool FindStandableFloor(const UWorld* World, const FVector& Point, const AActor* IgnoredActor, float MaxFloorAngle, FVector& OutFloor,
		float SearchUp = 300.0f, float SearchDown = 1500.0f);

	/** Room for a piece standing on Floor: two spheres of Radius at 60 and 140 cm above it hit no WorldStatic (walls, props). */
	static bool HasStandingRoom(const UWorld* World, const FVector& Floor, const AActor* IgnoredActor, float Radius);

	/** Points along the straight top-down line From → To, every PathSampleSpacing, each on the ground under it (trace from
	 * StepUpHeight above the previous point down to 600 below): low obstacles are ridden over, tall walls and gaps (no
	 * floor) are bridged with an arc of up to JumpArcHeight. Same rule on every machine (the server's is replicated). */
	void BuildGroundPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;
	/** Point at Alpha (0..1 of the path length) on the Catmull-Rom curve through Path. */
	static FVector EvaluatePath(const TArray<FVector_NetQuantize10>& Path, float Alpha);

	float GetSquareSize() const { return SquareSize; }
	float GetSnapSize() const { return SnapSize; }
	float GetMinPieceSpacing() const { return MinPieceSpacing; }
	float GetMaxFloorAngle() const { return MaxFloorAngle; }
	float GetPieceRadius() const { return PieceRadius; }
	/** The square it holds: its move's destination while moving. */
	virtual FVector GetBoardLocation() const;
	/** False = other pieces ignore it for spacing (IsSpotFree): the knight rides on a player. */
	virtual bool BlocksBoard() const { return true; }
	/** The player this piece rides on (the knight / king), or null for a piece on the board. */
	virtual AZeroLockCharacter* GetCarriedHero() const { return nullptr; }

	virtual bool IsMoving() const;
	/** Not moving and the per-piece cooldown is over (server clock, so the owning client agrees). */
	bool CanMove() const;
	float GetMoveCooldownRemaining() const;
	float GetMoveDistance() const { return MoveDistance; }

	/** The square a move toward Desired ends on (client preview and server use the same rule). False = nowhere to go. */
	virtual bool GetMoveTarget(const FVector& Desired, FVector& OutTarget) const;

	/** Server. Re-runs GetMoveTarget on Target; false = refused. */
	virtual bool ServerStartMove(const FVector& Target);

	/** The way a move to End would go, for the owner's preview (the default: BuildGroundPath from the piece). */
	virtual void GetPreviewPath(const FVector& End, TArray<FVector>& OutPath) const;

	/** Local only: tint on the owner's screen. */
	void SetHighlight(EZL_PieceHighlight InHighlight);

protected:
	UPROPERTY(VisibleAnywhere, Category = "ChessPiece")
	TObjectPtr<USceneComponent> Root;

	/** Placeholder look (engine shapes tinted PieceColor). Set any mesh in the BP; pivot on the floor. */
	UPROPERTY(VisibleAnywhere, Category = "ChessPiece")
	TObjectPtr<UStaticMeshComponent> PieceMesh;

	/** Edge of one of the bishop's squares (cm; Deadlock bishop Radius 3.5 m). */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Board")
	float SquareSize = 245.0f;

	/** Grid that placement and moves snap to (cm; 70 = 1 Deadlock m). 0 = no snapping. */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Board", meta = (ClampMin = 0))
	float SnapSize = 70.0f;

	/** Placement and moves end on the shared world board (SquareSize squares). False = the fine SnapSize grid. */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Board")
	bool bSnapToBoard = true;

	/** Closest two of his pieces can be (cm, Chebyshev): one square by default. */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Board", meta = (ClampMin = 0))
	float MinPieceSpacing = 245.0f;

	/** Steepest floor a piece can stand on (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Board", meta = (ClampMin = 0, ClampMax = 89))
	float MaxFloorAngle = 45.0f;

	/** Body radius for HasStandingRoom: a move can't end (nor a piece be placed) closer than this to a wall. */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Board", meta = (ClampMin = 1))
	float PieceRadius = 35.0f;

	/** Spacing of the ground path samples (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Move|Path", meta = (ClampMin = 10))
	float PathSampleSpacing = 50.0f;

	/** Highest step the path climbs onto from one sample to the next (cm); taller = a wall, bridged. */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Move|Path")
	float StepUpHeight = 150.0f;

	/** Peak of the arc over a bridged stretch (gap or tall wall), at its middle (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Move|Path")
	float JumpArcHeight = 80.0f;

	/** Range of one move: a sphere of this radius around the piece (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Move")
	float MoveDistance = 840.0f;

	/** cm/s. */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Move")
	float MoveSpeed = 2667.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Move")
	float MinMoveDuration = 0.15f;

	/** Seconds after a move ends before this piece can be moved again. */
	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Move")
	float MoveCooldown = 3.0f;


	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Look")
	FLinearColor PieceColor = FLinearColor(0.9f, 0.9f, 0.85f);

	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Look")
	FLinearColor HoverColor = FLinearColor(1.0f, 0.95f, 0.5f);

	UPROPERTY(EditDefaultsOnly, Category = "ChessPiece|Look")
	FLinearColor SelectedColor = FLinearColor(1.0f, 0.55f, 0.05f);

	UPROPERTY(ReplicatedUsing = OnRep_Move)
	FZL_ChessPieceMove Move;

	/** Server clock time from which the piece can move again. */
	UPROPERTY(Replicated)
	float NextMoveTime = 0.0f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_Move();

	/** The path a move From → To follows (ServerStartMove and the default preview). Default: BuildGroundPath. */
	virtual void BuildMovePath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;

	/** Server. Starts a move along Path (at least 2 points) over Duration: replicated, cooldown set, OnMoveStarted. */
	void StartMoveAlong(const TArray<FVector>& Path, float Duration);

	virtual void OnMoveStarted() {}
	/** Server, each tick of a move: the piece went From → To. */
	virtual void OnMoveStep(const FVector& From, const FVector& To) {}
	virtual void OnMoveFinished() {}

	float GetServerTime() const;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PieceMaterial;

	/** Server: the current move hasn't reached its end yet. */
	bool bMoveInProgress = false;
	EZL_PieceHighlight Highlight = EZL_PieceHighlight::None;

	UFUNCTION()
	void OnHeroDestroyed(AActor* DestroyedActor);
	void OnHeroPreDeath(AZeroLockCharacter* DeadHero);
	FDelegateHandle PreDeathHandle;
};
