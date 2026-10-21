// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Mover/ZeroMovementData.h"
#include "ZeroMoverComponent.generated.h"

class UNiagaraSystem;
struct FZeroMovementInputs;

/** Stamina after a (non-resimulated) sim step. Fired on the server and the owning client. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnZeroStaminaChanged, float /*Stamina*/, float /*MaxStamina*/);

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
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	int32 MaxAirJumps = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	int32 VerticalJumpForce = 400;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float DashSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float DashDuration = 0.2f;

	/** Ground dash only: added after DashDuration, blending from DashSpeed down to the walking MaxSpeed so Walking doesn't brake it to a dead stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float GroundDashEaseOutDuration = 0.2f;

	/** Dashes allowed per airtime (any mode other than Walking/Sliding); refills on landing. Negative = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	int32 MaxAirDashes = 1;

	/** Leaving a slide into Walking above MaxSpeed blends speed down to MaxSpeed over this long instead of Walking braking it in ~30 ms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	float SlideExitEaseOutDuration = 0.25f;

	/** Leaving a slide with no move input brakes to a stop at this rate (from any speed) instead of Walking braking it in ~0.1 s. Move input at or below MaxSpeed, or leaving Walking, hands control back. 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings", meta = (ForceUnits = "cm/s²"))
	float SlideExitBrakingDeceleration = 1200.0f;

	/** No jumping, dashing or mantling while in this mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Settings")
	FName MeleeModeName = FName("Melee");

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

	/** Air jumps and dashes cost stamina. Max and regen come from the owner's MaxStamina / StaminaRegenRate attributes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Stamina")
	bool bUseStamina = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Stamina")
	float DashStaminaCost = 1.0f;

	/** Cost of a jump started while Falling. Ground jumps are free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Stamina")
	float AirJumpStaminaCost = 1.0f;

	/** Used when the owner has no stamina attributes (pawns without the hero attribute set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Stamina")
	float DefaultMaxStamina = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Stamina")
	float DefaultStaminaRegenRate = 0.2f;

	/** Stamina at the end of the newest simulated frame (predicted on the owning client). */
	float GetPredictedStamina() const;
	float GetMaxStamina() const;

	/** Server: fill stamina on the next sim step (respawn). The client is corrected to match. */
	void RequestStaminaRefill() { bPendingStaminaRefill = true; }

	FOnZeroStaminaChanged OnStaminaChanged;

	void RequestSafePullTo(const FVector& Target, float Duration);
	void RequestSafeStop();
	void RequestSafeRelease();
	void RequestSafeTeleport(const FVector& Target);
	/** Teleport that also sets the velocity (e.g. leaving a portal). */
	void RequestSafeTeleportWithVelocity(const FVector& Target, const FVector& Velocity);

	bool bLatchedPullMove = false;
	FVector LatchedPullTarget = FVector::ZeroVector;
	float LatchedPullDuration = 0.f;
	bool bLatchedStop = false;
	bool bLatchedRelease = false;
	bool bLatchedTeleport = false;
	FVector LatchedTeleportTarget = FVector::ZeroVector;
	bool bLatchedTeleportSetsVelocity = false;
	FVector LatchedTeleportVelocity = FVector::ZeroVector;

	/** Teleports further than this from the current location are rejected (client-sent target sanity check). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Teleport")
	float MaxTeleportDistance = 10000.0f;

	/** Mode the pawn is held in while rooted. Must ignore move input (e.g. a ZeroFloatMode without bPreserveMomentum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Rooted")
	FName RootedModeName = FName("Locked");

	/** Mode queued on the frame the pawn stops being rooted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Rooted")
	FName RootedExitModeName = FName("Falling");

	/**
	 * True while the owner has the ZeroLock.Movement.Rooted tag. Only read when producing input:
	 * the simulation uses FZeroMovementInputs::bIsRooted so client and server root on the same frame.
	 */
	bool IsRooted() const;

	/** True while the owner has the ZeroLock.Stun tag: ProduceInput drops the player's own move inputs (latched forced moves still apply). */
	bool IsStunned() const;

	/** True while the current sim step re-runs a frame after a correction. Use it to skip cosmetics. */
	bool IsResimulatingStep() const { return bStepIsResimulating; }

protected:
	void HandleRooted(const FMoverTimeStep& TimeStep, const FName& CurrentMode);

	void HandleTeleportInput(const FZeroMovementInputs& ZeroInputs);

	bool HandleWallBounceCheck(const FZeroMovementInputs& ZeroInputs,const FName& CurrentMode);

	/** Queues the dash and returns its total duration in ms. */
	float HandleDashInputs(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs, const FName& CurrentMode);

	/** Slide → Walking above MaxSpeed: ease the extra speed off instead of letting Walking brake it instantly. */
	void HandleSlideExit(const FName& CurrentMode, const FCharacterDefaultInputs& DefaultInputs);

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashForwardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashBackwardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashLeftMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animations")
	UAnimMontage* DashRightMontage;

	void PlayDirectionalDashMontage(const FVector& DashDirection);

public:
	/** Increments each time a dash starts on this machine (not during resimulation). Read by the Deadlock anim instance. */
	int32 DashCount = 0;

	/** World-space direction of the last dash. */
	FVector LastDashDirection = FVector::ForwardVector;

protected:


	/** Resets the jump counter on the ground; jumps / wall bounces only when bAllowJump. */
	void HandleAirJumpTracking(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs, bool bAllowJump);

	/** Sets the crouch request from this frame's input before the base class applies stance changes. */
	void UpdateCrouchIntent(const FName& CurrentMode, const FZeroMovementInputs& ZeroInputs);

	bool TryMantle(const FCharacterDefaultInputs& DefaultInputs, const FZeroMovementInputs& ZeroInputs);

	bool ShouldIgnoreServerLatch() const;

	/** Reserves stamina for this sim step. Returns false (and spends nothing) if there isn't enough. */
	bool TrySpendStamina(float Cost);

	/** Velocity / location at the start of the current sim step. GetSyncState() is stale during resimulation and multi-step frames. */
	FVector GetSimStartVelocity() const;
	FVector GetSimStartLocation() const;

	UFUNCTION()
	void HandlePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& SyncState, FMoverAuxStateContext& AuxState);

	UFUNCTION()
	void HandleSimRollback(const FMoverTimeStep& NewBaseTimeStep, const FMoverTimeStep& ExpungedTimeStep);

private:
	void BeginSimStep(const FMoverTimeStep& TimeStep);
	void CacheSimStartState(const FMoverSyncState& SyncState);
	float GetStartStamina() const;

	// Start-of-step state, refreshed after every step and on rollback.
	FZeroMoverSimState SimStartState;
	FVector SimStartLocation = FVector::ZeroVector;
	FVector SimStartVelocity = FVector::ZeroVector;
	bool bHasSimStartCache = false;

	// Filled during OnMoverPreSimulationTick, written to the sync state in HandlePostMovement of the same step.
	float PendingStaminaSpend = 0.0f;
	int32 PendingAirJumpsUsed = 0;
	int32 PendingAirDashesUsed = 0;
	float PendingDashRemainingMs = 0.0f;
	bool bPendingRooted = false;
	bool bPendingSliding = false;
	bool bPendingStaminaRefill = false;
	float StepMaxStamina = 0.0f;
	float StepStaminaRegenRate = 0.0f;
	float StepSeconds = 0.0f;

	double NewestSimTimeMs = -1.0;
	bool bStepIsResimulating = false;

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
