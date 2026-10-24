// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroCubedMode.generated.h"

/**
 * Encased in Viscous's Cube: a slippery block with no input of its own. On ground-like surfaces (normal Z above
 * GroundMinNormalZ) gravity pulls it down the slope and Friction slows it; in the air it falls; walls bounce it back
 * with Restitution. Layered moves (pulls, stops) override the velocity as usual; pushes come in as additive impulses
 * (UZeroMoverComponent turns ability moves into FApplyVelocityEffect while cubed). Doesn't turn.
 * UZeroMoverComponent adds it at runtime as CubedModeName if the Blueprint's mode map doesn't have it.
 */
UCLASS()
class ZEROLOCK_API UZeroCubedMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	/** Speed lost per second while sliding on the ground (cm/s²). Low = slippery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubed", meta = (ForceUnits = "cm/s²"))
	float Friction = 150.0f;

	/** Share of the speed into a wall that bounces back (0 = stops dead against it, 1 = full bounce). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubed", meta = (ClampMin = 0, ClampMax = 1))
	float Restitution = 0.35f;

	/** Surfaces whose normal Z is at least this count as ground (slide on them); steeper ones are walls (bounce). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubed", meta = (ClampMin = 0, ClampMax = 1))
	float GroundMinNormalZ = 0.25f;

	/** Within this distance of a floor the cube counts as resting on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubed", meta = (ForceUnits = "cm"))
	float GroundSnapDistance = 3.0f;
};
