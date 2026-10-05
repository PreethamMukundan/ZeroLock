// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "DefaultMovementSet/Modes/FlyingMode.h"
#include "ZeroFloatMode.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroFloatMode : public UFlyingMode
{
	GENERATED_BODY()

public:
	/** If true, the velocity the pawn entered this mode with is kept (no deceleration), and move input only steers it slightly (see SteerAcceleration). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float")
	bool bPreserveMomentum = false;

	/** With bPreserveMomentum, drops the vertical (gravity-relative) part of the velocity so the pawn floats level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float", meta = (EditCondition = "bPreserveMomentum"))
	bool bIgnoreVerticalMomentum = true;

	/** With bPreserveMomentum, how hard move input pushes the pawn (cm/s^2). 0 ignores input entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float", meta = (EditCondition = "bPreserveMomentum", ClampMin = "0"))
	float SteerAcceleration = 800.0f;

	/** Steering can build speed up to this. Faster entry momentum can only be redirected or slowed by it, never sped up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float", meta = (EditCondition = "bPreserveMomentum", ClampMin = "0"))
	float MaxSteerSpeed = 300.0f;

protected:
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

};
