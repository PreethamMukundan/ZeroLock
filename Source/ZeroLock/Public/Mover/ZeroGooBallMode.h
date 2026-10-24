// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroGooBallMode.generated.h"

/**
 * Viscous's Goo Ball: a heavy, bouncy ball the player steers. On ground-like surfaces it accelerates toward the move
 * input at RollAcceleration (up to MaxRollSpeed), but its direction only turns at TurnRate, so it drifts through turns;
 * without input RollFriction slowly stops it; slopes speed it up downhill. In the air AirControl steers a little.
 * Walls bounce it back with WallRestitution, landings bounce with FloorRestitution above MinBounceSpeed.
 * Jumps (ground + air) are queued by UZeroMoverComponent as impulses; layered moves override the velocity as usual.
 * UZeroMoverComponent adds it at runtime as GooBallModeName if the Blueprint's mode map doesn't have it.
 */
UCLASS()
class ZEROLOCK_API UZeroGooBallMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ForceUnits = "cm/s²"))
	float RollAcceleration = 1400.0f;

	/** Input can't push it faster than this (slopes and pushes can). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ForceUnits = "cm/s"))
	float MaxRollSpeed = 1300.0f;

	/** How fast the rolling direction can turn toward the input (degrees per second). Low = hard to steer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball")
	float TurnRate = 120.0f;

	/** Speed lost per second on the ground without input (cm/s²). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ForceUnits = "cm/s²"))
	float RollFriction = 250.0f;

	/** Input braking when steering against the roll (cm/s², on top of friction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ForceUnits = "cm/s²"))
	float BrakeDeceleration = 600.0f;

	/** Air steering acceleration (cm/s²). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ForceUnits = "cm/s²"))
	float AirControl = 400.0f;

	/** Share of the speed into a wall that bounces back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ClampMin = 0, ClampMax = 1))
	float WallRestitution = 0.8f;

	/** Share of the falling speed that bounces back on landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ClampMin = 0, ClampMax = 1))
	float FloorRestitution = 0.35f;

	/** Landings slower than this don't bounce (no jitter on the ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ForceUnits = "cm/s"))
	float MinBounceSpeed = 400.0f;

	/** Surfaces whose normal Z is at least this count as ground; steeper ones are walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ClampMin = 0, ClampMax = 1))
	float GroundMinNormalZ = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goo Ball", meta = (ForceUnits = "cm"))
	float GroundSnapDistance = 4.0f;
};
