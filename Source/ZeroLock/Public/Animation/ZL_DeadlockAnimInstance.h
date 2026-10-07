// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ZL_DeadlockAnimInstance.generated.h"

class AZeroMoverPawn;
class UCharacterMoverComponent;
class UZL_DeadlockAnimSet;
struct FZLDirectionalAnims;

/** Movement state gathered from the Mover component each frame, read by FAnimNode_ZLDeadlockHero. */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZLDeadlockAnimState
{
	GENERATED_BODY()

	/** Velocity in actor space (X forward, Y right), cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	FVector LocalVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	float VerticalSpeed = 0.f;

	/** Degrees, positive = looking up. */
	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	float AimPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	FName MovementMode;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	bool bOnGround = true;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	bool bCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	bool bSliding = false;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	bool bDashing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	bool bZiplining = false;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	bool bWallJumping = false;

	/** Increments on every jump, air jump and wall jump, so the anim graph can restart the jump clip. */
	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	int32 JumpCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	bool bLastJumpWasAirJump = false;

	/** Increments on every dash. ZeroLock dashes are a velocity burst (layered move), not a movement mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	int32 DashCount = 0;

	/** Direction of the last dash in actor space (X forward, Y right). */
	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	FVector DashDirection = FVector::ForwardVector;

	/** Increments on landing after a meaningful time in the air. */
	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	int32 LandCount = 0;

	/** Downward speed at the last landing, cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	float LastLandingSpeed = 0.f;

	/** Play-rate reference speeds for the active anim set, measured from root motion where possible. */
	float RunAuthoredSpeed = 600.f;
	float CrouchAuthoredSpeed = 300.f;

	/** Where SlideStart begins when a slide starts from the air (skips its hop), resolved from the active anim set. */
	float SlideFromAirStartTime = 0.f;
};

/**
 * Anim instance for ABP_DeadlockHero, the master anim blueprint shared by every Deadlock hero.
 * Reads the pawn's Mover component; the pose itself is built by the "Deadlock Hero" anim node.
 * The hero's clips come from AnimSet if set here, otherwise from AZeroMoverPawn::DeadlockAnimSet.
 */
UCLASS(Blueprintable)
class ZEROLOCK_API UZL_DeadlockAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Overrides the pawn's DeadlockAnimSet (e.g. in a hero-specific child anim blueprint). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadlock")
	TObjectPtr<UZL_DeadlockAnimSet> AnimSet;

	UPROPERTY(BlueprintReadOnly, Category = "Deadlock")
	FZLDeadlockAnimState State;

	UFUNCTION(BlueprintCallable, Category = "Deadlock")
	void SetAnimSet(UZL_DeadlockAnimSet* NewAnimSet);

	UFUNCTION(BlueprintPure, Category = "Deadlock")
	UZL_DeadlockAnimSet* GetActiveAnimSet() const { return ActiveAnimSet; }

	/** Mover mode names, matching the ZeroLock movement modes. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadlock|Modes")
	FName SlidingModeName = TEXT("Sliding");

	UPROPERTY(EditDefaultsOnly, Category = "Deadlock|Modes")
	FName DashingModeName = TEXT("Dashing");

	UPROPERTY(EditDefaultsOnly, Category = "Deadlock|Modes")
	FName ZipliningModeName = TEXT("Ziplining");

	UPROPERTY(EditDefaultsOnly, Category = "Deadlock|Modes")
	FName WallJumpingModeName = TEXT("WallJumping");

	UPROPERTY(EditDefaultsOnly, Category = "Deadlock|Modes")
	FName AirJumpingModeName = TEXT("AirJumping");

	/** A dash is detected when horizontal speed exceeds this fraction of UZeroMoverComponent::DashSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadlock", meta = (ClampMin = "0.1", ClampMax = "1"))
	float DashDetectSpeedRatio = 0.75f;

	/** Bone whose translation carries the clip's root motion (stripped by the anim node). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadlock")
	FName RootMotionBoneName = TEXT("root_motion");

	/** Used to find the impact frame of SlideStart (its lowest pelvis point). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadlock")
	FName PelvisBoneName = TEXT("pelvis");

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void ResolveAnimSet();
	float MeasureCycleSpeed(const FZLDirectionalAnims& Cycle) const;
	float MeasureSlideLandTime(const UAnimSequence* Seq) const;

	UPROPERTY(Transient)
	TObjectPtr<UZL_DeadlockAnimSet> ActiveAnimSet;

	TWeakObjectPtr<AZeroMoverPawn> OwnerPawn;
	TWeakObjectPtr<UCharacterMoverComponent> Mover;

	bool bWasOnGround = true;
	bool bWasDashBurst = false;
	int32 SeenMoverDashCount = 0;
	float TimeSinceDash = 10.f;
	float PrevVerticalSpeed = 0.f;
	float AirTime = 0.f;
	float MaxFallSpeed = 0.f;
	FName PrevMode;
};
