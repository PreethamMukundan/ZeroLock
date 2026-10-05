// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "ZeroMoverComponent.generated.h"

class UNiagaraSystem;
struct FZeroMovementInputs;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZeroMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()
	
public:
	UZeroMoverComponent();
	
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd) override;
	virtual void InitializeComponent() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	int32 MaxAirJumps = 2;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	int32 VerticalJumpForce = 400;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float DashSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float DashDuration = 0.2f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float WallJumpOffForce = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float WallJumpVerticalForce = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float WallBounceTracePadding = 35.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Mantle")
	float MantleReachHeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Mantle")
	float MantleMaxDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Mantle")
	float MantleMinWallSteepnessAngle = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Mantle")
	float MantleMaxSurfaceAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Mantle")
	float MantleMaxAlignmentAngle = 45.0f;

	void RequestSafePullTo(const FVector& Target, float Duration);
	void RequestSafeStop();
	void RequestSafeRelease();
	void RequestSafeTeleport(const FVector& Target);

	bool bLatchedPullMove = false;
	FVector LatchedPullTarget = FVector::ZeroVector;
	float LatchedPullDuration = 0.f;
	bool bLatchedStop = false;
	bool bLatchedRelease = false;
	bool bLatchedTeleport = false;
	FVector LatchedTeleportTarget = FVector::ZeroVector;

	/** Teleports further than this from the current location are rejected (client-sent target sanity check). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Teleport")
	float MaxTeleportDistance = 10000.0f;

	/** Mode the pawn is held in while rooted. Must ignore move input (e.g. a ZeroFloatMode without bPreserveMomentum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Rooted")
	FName RootedModeName = FName("Locked");

	/** True while the owner has the ZeroLock.Movement.Rooted tag: the pawn is held in place and only teleports are applied. */
	bool IsRooted() const;


protected:
	void HandleRooted(const FMoverTimeStep& TimeStep, const FName& CurrentMode);

	void HandleTeleportInput(const FZeroMovementInputs& ZeroInputs);

	bool HandleWallBounceCheck(const FZeroMovementInputs& ZeroInputs,const FName& CurrentMode);

	void HandleDashInputs(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs);
	
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashForwardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashBackwardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashLeftMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashRightMontage;
	
	void PlayDirectionalDashMontage(const FVector& DashDirection);
	
	
	void HandleAirJumpTracking(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs);

	void HandleCrouching(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs);
		
	bool TryMantle(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs);
	
	bool ShouldIgnoreServerLatch() const;
private:
	int32 LocalAirJumpsUsed = 0;
	
public:
	
	UFUNCTION(BlueprintCallable, Category = "ZeroLock|Movement")
	void RequestSafeAbilityMove(FVector Velocity, float Duration);
	
	UFUNCTION(BlueprintCallable, Category = "ZeroLock|Movement")
	void RequestSafeDynamicAbilityMove(AActor* TargetActor, float Duration);


	bool bLatchedAbilityMove = false;
	FVector LatchedAbilityVelocity = FVector::ZeroVector;
	float LatchedAbilityDuration = 0.0f;
	
	bool bLatchedDynamicMove = false;
	UPROPERTY()
	AActor* LatchedDynamicActor = nullptr;
	float LatchedDynamicDuration = 0.0f;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "WallBounce|VFX")
	UNiagaraSystem* VFX_WallBounce;
	
	UPROPERTY(EditDefaultsOnly, Category = "AirJump|VFX")
	UNiagaraSystem* VFX_AirJump;
	
};
