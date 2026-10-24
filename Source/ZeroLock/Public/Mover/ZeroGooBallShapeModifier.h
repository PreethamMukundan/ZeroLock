// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "ZeroGooBallShapeModifier.generated.h"

/**
 * Viscous's Goo Ball: while active the pawn's capsule is a sphere of BallRadius (radius = half height), like the
 * engine's crouch FStanceModifier: part of the predicted sync state, so client and server resize on the same frame.
 * OnStart/OnEnd resize the capsule and teleport it so its bottom stays where it was. Queued and cancelled by
 * UZeroMoverComponent from FZeroMovementInputs::bIsGooBall.
 */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZeroGooBallShapeModifier : public FMovementModifierBase
{
	GENERATED_BODY()

	FZeroGooBallShapeModifier();
	virtual ~FZeroGooBallShapeModifier() override = default;

	UPROPERTY(BlueprintReadWrite, Category = "Goo Ball")
	float BallRadius = 90.0f;

	virtual void OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	virtual void OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

private:
	static void SetCapsule(UMoverComponent* MoverComp, float NewRadius, float NewHalfHeight);
};
