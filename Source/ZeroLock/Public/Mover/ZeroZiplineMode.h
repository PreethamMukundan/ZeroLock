// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "ZeroZiplineMode.generated.h"

class AZero_ZiplineActor;

USTRUCT(BlueprintType)
struct FZeroZipliningState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	TObjectPtr<AActor> ZiplineActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	uint8 bMovingToEnd : 1;

	// Distance along the zipline's spline (cm), advanced by the mode; not re-projected from the pawn every tick.
	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	float Distance = 0.0f;

	// Current speed along the cable (cm/s).
	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	float Speed = 0.0f;

	// Sim time of the last zipline step. A gap means a fresh grab; the grab transition also uses it as a regrab cooldown.
	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	double LastZipSimTimeMs = -1.0;

	FZeroZipliningState() : bMovingToEnd(true) {}

	// FMoverDataStructBase Overrides
	virtual FMoverDataStructBase* Clone() const override { return new FZeroZipliningState(*this); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	virtual UScriptStruct* GetScriptStruct() const override;
};

/**
 * Rides an AZero_ZiplineActor's spline. Grabs the cable the player is looking at (sphere sweep along the look direction,
 * long enough to look up at a cable from the ground), hangs HangOffset below it (pulled up at AttachSpeed), speeds up
 * from the entry speed to ZiplineSpeed, and keeps its velocity when it leaves (cable end, jump, crouch, blocked).
 */
UCLASS()
class ZEROLOCK_API UZeroZiplineMode : public UBaseMovementMode
{
	GENERATED_BODY()
public:
	UZeroZiplineMode(const FObjectInitializer& ObjectInitializer);

	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	/**
	 * First zipline cable along the look ray: sphere sweep of TraceRadius from Eye along LookDir up to MaxDistance on the
	 * Vehicle object type. OutDistance = distance along its spline of the point closest to the hit.
	 */
	static AZero_ZiplineActor* FindZiplineInView(const UWorld* World, const FVector& Eye, const FVector& LookDir,
		float MaxDistance, float TraceRadius, const AActor* IgnoreActor, float& OutDistance);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float ZiplineSpeed = 1200.0f;

	// cm/s² from the entry speed up to ZiplineSpeed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float ZiplineAcceleration = 1500.0f;

	// The ride never starts slower than this, whatever the velocity at the grab.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float MinEntrySpeed = 600.0f;

	// The pawn's centre hangs this far below the cable (capsule half height + a hand's reach).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float HangOffset = 140.0f;

	// Extra speed toward the hang point while it hasn't been reached yet (the pull up to the cable at a grab).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float AttachSpeed = 2500.0f;

	// Look trace at a grab (eye = pawn centre + EyeHeight). A bit longer and wider than the transition's, so a grab it
	// allowed always finds the cable again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float LookMaxDistance = 2700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float LookTraceRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	float EyeHeight = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	FName ZipliningModeName = TEXT("Ziplining");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zipline Settings")
	FName ExitModeName = TEXT("Falling");
};
