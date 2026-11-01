// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Solomon_Rook.generated.h"

class AZeroLockCharacter;
class UStaticMeshComponent;

/** Where a rook is and where it's going, set by the server when it starts, reflects or forks. */
USTRUCT()
struct FZL_RookState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	/** Flat. */
	UPROPERTY()
	FVector_NetQuantizeNormal Heading = FVector::ForwardVector;

	/** AGameStateBase::GetServerWorldTimeSeconds at Location. */
	UPROPERTY()
	float ServerTime = 0.0f;

	/** Flat distance it still travels. */
	UPROPERTY()
	float DistanceLeft = 0.0f;

	UPROPERTY()
	uint8 ReflectsLeft = 0;

	UPROPERTY()
	bool bForked = false;

	/** Bounced off a wall at Location: heading back along the path it came. */
	UPROPERTY()
	bool bRetracing = false;

	/** Bumped per change so every one replicates. */
	UPROPERTY()
	uint8 Id = 0;
};

/**
 * One of the two rooks of Solomon's Rook Battery (UZL_Solomon_RookBattery). Spawned by the server, owned and instigated
 * by Solomon; not a chess piece (Move Piece doesn't move it).
 *
 * Slides along the ground (like the Luggage Cart: snaps to walkable floor within GroundSnapDistance, climbs steps up to
 * MaxStepHeight, falls off ledges) at Speed along a flat heading for MaxTravelDistance. A wall bounces it back along
 * the exact path it came (the recorded leg, played backwards; always the whole way back, even past MaxTravelDistance),
 * then on in that direction with the distance left. MaxReflects bounces, then a wall ends it. Fork (the ability's
 * recast, server) turns it to ForkHeading for ForkDistance more (also mid-bounce).
 * Every machine runs the same movement in Tick; the server replicates FZL_RookState at each start, reflect and fork, and
 * clients jump to it and fast-forward by the time it took to arrive. No movement replication.
 *
 * Server: each enemy hero its hit capsule (HitRadius, HitHeight) passes takes Damage spirit and is rooted
 * (ZeroLock.Movement.Rooted) for ImmobilizeDuration. Once per heading: a reflect or fork can hit them again, but never
 * within ImmunityDuration of a hit by this rook or its Partner.
 */
UCLASS()
class ZEROLOCK_API AZL_Solomon_Rook : public AActor
{
	GENERATED_BODY()

public:
	AZL_Solomon_Rook();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, between SpawnActorDeferred and FinishSpawning. */
	void InitRook(float InDamage, float InImmobilizeDuration, float InSpeed, float InMaxTravelDistance, float InForkDistance,
		int32 InMaxReflects, const FVector& InForkHeading);

	/** Server. The other rook of the pair (shared hit immunity). */
	void SetPartner(AZL_Solomon_Rook* InPartner) { Partner = InPartner; }

	/** Server. Turn to ForkHeading for ForkDistance more. Once. */
	void Fork();

	bool HasForked() const { return State.bForked; }

	AZeroLockCharacter* GetHero() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Rook")
	TObjectPtr<USceneComponent> Root;

	/** Placeholder look. */
	UPROPERTY(VisibleAnywhere, Category = "Rook")
	TObjectPtr<UStaticMeshComponent> RookMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	FLinearColor RookColor = FLinearColor(0.15f, 0.35f, 1.0f);

	/** cm/s. Deadlock projectile speed 850 u/s. Overridden by InitRook. */
	UPROPERTY(EditDefaultsOnly, Category = "Rook|Move")
	float Speed = 1511.0f;

	/** Sphere swept against WorldStatic for floors and walls; the root sits this high above the floor. */
	UPROPERTY(EditDefaultsOnly, Category = "Rook|Move")
	float RookRadius = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rook|Move")
	float GroundSnapDistance = 40.0f;

	/** Curbs and stair steps up to this are climbed; taller is a wall. */
	UPROPERTY(EditDefaultsOnly, Category = "Rook|Move")
	float MaxStepHeight = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rook|Move")
	float MaxWalkableSlopeAngle = 50.0f;

	/** Off a ledge (cm/s²). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook|Move")
	float Gravity = 2500.0f;

	/** Hit capsule radius (Deadlock trigger radius 70 u). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook|Hit")
	float HitRadius = 124.0f;

	/** Hit capsule height from the floor (Deadlock trigger capsule 80 u tall + radius). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook|Hit")
	float HitHeight = 266.0f;

	/** No second hit on the same enemy by either rook within this (Deadlock ImmunityDuration 0.75). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook|Hit")
	float ImmunityDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	bool bDrawDebug = false;

private:
	UPROPERTY(ReplicatedUsing = OnRep_State)
	FZL_RookState State;

	UFUNCTION()
	void OnRep_State();

	/** Set by InitRook (server only). */
	float Damage = 50.0f;
	float ImmobilizeDuration = 1.0f;
	float ForkDistance = 840.0f;
	FVector ForkHeading = FVector::RightVector;

	/** Local simulation (every machine). */
	FVector Heading = FVector::ForwardVector;
	float VerticalSpeed = 0.0f;
	float DistanceLeft = 0.0f;
	int32 ReflectsLeft = 0;
	bool bFinished = false;
	uint8 AppliedStateId = 0;
	/** Locations since this leg started (start, fork or the end of a bounce), for a bounce to retrace. */
	TArray<FVector> LegPath;
	/** While bouncing back: the leg being retraced, walked from RetraceIndex down to 0. */
	TArray<FVector> RetracePath;
	int32 RetraceIndex = INDEX_NONE;

	TWeakObjectPtr<AZL_Solomon_Rook> Partner;
	/** Enemies hit on the current heading. */
	TSet<TWeakObjectPtr<AZeroLockCharacter>> HitThisLeg;
	/** Last hit time per enemy (world time), for ImmunityDuration. */
	TMap<TWeakObjectPtr<AZeroLockCharacter>, float> LastHitTimes;

	float GetServerTime() const;
	/** Server: the current simulation into State. */
	void PushState();
	/** Every machine: one movement step. */
	void StepRook(float DeltaSeconds);
	/** Wall in the way: bounce back along the leg, or finish. */
	void Reflect(const FVector& WallLocation);
	/** Start walking LegPath (+ WallLocation) backwards. */
	void BeginRetrace(const FVector& WallLocation);
	void StepRetrace(float DeltaSeconds);
	void EndRetrace();
	bool IsRetracing() const { return RetraceIndex != INDEX_NONE; }
	void StartLeg();
	void Finish();

	bool IsWalkable(const FVector& Normal) const;
	bool SweepStatic(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	bool TryStepUp(const FVector& From, const FVector& To, FVector& OutLocation) const;

	/** Server. */
	void SweepHits(const FVector& From, const FVector& To);
	void HitVillan(AZeroLockCharacter* Villan);
	bool IsImmune(const AZeroLockCharacter* Villan, float Now) const;
};
