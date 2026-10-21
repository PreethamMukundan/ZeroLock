// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "ZeroInstantMovementEffects.generated.h"

/** Teleport that also replaces the velocity, in the same sim step (portals redirect momentum). */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZeroTeleportWithVelocityEffect : public FTeleportEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector Velocity = FVector::ZeroVector;

	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override { return new FZeroTeleportWithVelocityEffect(*this); }
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override { return FZeroTeleportWithVelocityEffect::StaticStruct(); }
	virtual FString ToSimpleString() const override { return TEXT("ZeroTeleportWithVelocity"); }
};
