// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroNoClipMode.generated.h"

/**
 * Cheat noclip (UZL_CheatComponent): flies along the camera (forward/back follow the look pitch, strafe stays flat),
 * jump held = up, crouch held = down, and moves without sweeping, so it passes through everything. No gravity; stops
 * as soon as there is no input. Driven by FZeroMovementInputs::bIsNoClip, so it is predicted like any other input.
 * UZeroMoverComponent adds it at runtime as NoClipModeName if the Blueprint's mode map doesn't have it.
 */
UCLASS()
class ZEROLOCK_API UZeroNoClipMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	/** Flight speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NoClip", meta = (ForceUnits = "cm/s"))
	float Speed = 1500.0f;
};
