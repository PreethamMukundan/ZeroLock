// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroSlideMode.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroSlideMode : public UBaseMovementMode
{
	GENERATED_BODY()
public:
	UZeroSlideMode(const FObjectInitializer& ObjectInitializer);

	//virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Movement", meta = (ForceUnits = "cm/s"))
	float SlideInitialSpeedBoost = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Movement", meta = (ForceUnits = "cm/s²"))
	float SlideGravityForce = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Movement", meta = (ForceUnits = "cm/s²"))
	float SlideSteeringAuthority = 2048.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Movement")
	float SlideFriction = 0.8f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Movement", meta = (ForceUnits = "cm/s"))
	float SlideMinSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Movement", meta = (ForceUnits = "cm/s"))
	float SlideJumpImpulse = 600.0f;
};
