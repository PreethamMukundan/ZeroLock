// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "ZeroDashMove.generated.h"

// Dash velocity override: full speed for the first (DurationMs - EaseOutMs), then blends linearly down to ExitSpeed.
// Ending at or below the walking MaxSpeed stops Walking from braking the leftover dash speed to a dead stop.
USTRUCT(BlueprintType)
struct FZeroLayeredMove_Dash : public FLayeredMoveBase
{
	GENERATED_BODY()

	FZeroLayeredMove_Dash() { MixMode = EMoveMixMode::OverrideVelocity; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zero|Dash")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zero|Dash")
	float EaseOutMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zero|Dash")
	float ExitSpeed = 0.0f;

	/** >= 0: the move ends when the mode leaves Walking, or on move input once its speed is at or below this, handing velocity back to the mode. Negative = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zero|Dash")
	float CancelOnMoveInputBelowSpeed = -1.0f;

	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override { return new FZeroLayeredMove_Dash(*this); }
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override { return FZeroLayeredMove_Dash::StaticStruct(); }
	virtual FString ToSimpleString() const override { return TEXT("ZeroDash"); }
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
};

template<>
struct TStructOpsTypeTraits<FZeroLayeredMove_Dash> : public TStructOpsTypeTraitsBase2<FZeroLayeredMove_Dash>
{
	enum
	{
		WithCopy = true
	};
};
