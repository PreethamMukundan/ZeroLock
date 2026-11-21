// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_Tempest_IAmNumber10.generated.h"

class AProjectile;
class AZL_Tempest_StormBall;
class UZL_WaitDelay_Task;

/**
 * Tempest's ultimate, I Am Number 10 ("Eye of the Storm"). Leaps up (LeapUpSpeed for LeapDuration, then stops) and hangs
 * in the air in HoldModeName (Mover "Locked") with other abilities blocked. After FireDelay, each confirm (left click; it
 * doesn't also fire the weapon) throws one AZL_Tempest_StormBall at the crosshair, up to NumBalls within ConfirmWindow
 * (progress bar). Jump or the ult key ends it early. The remote server starts NumBalls spawn tasks at activation; the
 * client ends it (replicated) after its last ball or when it stops, which cancels the unused tasks.
 */
UCLASS()
class ZEROLOCK_API UZL_Tempest_IAmNumber10 : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Tempest_IAmNumber10();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	float LeapUpSpeed = 1600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	float LeapDuration = 0.3f;

	/** Mover mode he hangs in after the leap. */
	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	FName HoldModeName = FName("Locked");

	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	TSubclassOf<AZL_Tempest_StormBall> ProjectileClass;

	/** Falls back to ProjectileClass. */
	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	TSubclassOf<AZL_Tempest_StormBall> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Balls", meta = (ClampMin = "1"))
	int32 NumBalls = 3;

	/** Seconds after the press before the first ball can be thrown. */
	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	float FireDelay = 0.35f;

	/** Seconds after FireDelay to throw them. */
	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	float ConfirmWindow = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	float SpawnForwardOffset = 150.0f;

	/** To each enemy the ball passes. */
	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	FScalableFloat DirectDamage = 80.0f;

	/** Each burst (each enemy passed and where it stops), with a pull to the burst point and a wind dome. */
	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	FScalableFloat BurstDamage = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Balls")
	FScalableFloat BurstRadius = 350.0f;

	/** On Tempest at the press (owner predicted + server). */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue CastCue;

private:
	void StartSpawnTask(const FVector& Location, const FRotator& Rotation);
	void ArmConfirm();
	void Stop();

	UFUNCTION()
	void OnLeapFinished();

	UFUNCTION()
	void OnConfirmPressed();

	UFUNCTION()
	void OnFireDelayFinished();

	UFUNCTION()
	void OnWindowFinished();

	UFUNCTION()
	void OnRepressed(float TimeWaited);

	void OnJumpPressed();

	UFUNCTION()
	void OnBallSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnBallFailed(AProjectile* SpawnedProjectile);

	void OnSpawnReported();

	UPROPERTY()
	TObjectPtr<UZL_WaitDelay_Task> WindowTask;

	FDelegateHandle JumpHandle;
	FTimerHandle ServerSafetyTimer;
	int32 BallsFired = 0;
	int32 PendingSpawns = 0;
	bool bHolding = false;
	bool bConfirmBound = false;
	bool bEnding = false;
};
