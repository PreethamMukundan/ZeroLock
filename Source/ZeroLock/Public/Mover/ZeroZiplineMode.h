// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroZiplineMode.generated.h"


USTRUCT(BlueprintType)
struct FZeroZipliningState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	TObjectPtr<AActor> ZiplineActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	uint8 bMovingToEnd : 1;

	// FMoverDataStructBase Overrides
	virtual FMoverDataStructBase* Clone() const override { return new FZeroZipliningState(*this); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	virtual UScriptStruct* GetScriptStruct() const override;
};
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroZiplineMode : public UBaseMovementMode
{
	GENERATED_BODY()
public:
	UZeroZiplineMode(const FObjectInitializer& ObjectInitializer);

	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float ZiplineSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	FName ZipliningModeName = TEXT("Ziplining");
};
