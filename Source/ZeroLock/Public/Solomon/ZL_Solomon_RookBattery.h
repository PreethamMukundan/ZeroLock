// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Solomon_RookBattery.generated.h"

class AZL_Solomon_Rook;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitInputPress;
class UAnimMontage;

/**
 * Solomon's Rook Battery (Deadlock ability_chessmaster_forkpiece): "Release a pair of rooks that travel forward, dealing
 * spirit damage and applying immobilize. If the rooks hit a wall, they reflect back in the opposite direction.
 * Re-activate: split the rooks perpendicularly."
 * Press: commits (cooldown starts), CastDelay later the server spawns two AZL_Solomon_Rook side by side (DeployGap apart)
 * at his feet, heading along the flat aim. The ability stays active while they're out: a re-press makes the server fork
 * them (left one turns left, right one right, relative to the cast) and ends it. The server ends it when both are gone.
 */
UCLASS()
class ZEROLOCK_API UZL_Solomon_RookBattery : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Solomon_RookBattery();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	TSubclassOf<AZL_Solomon_Rook> RookClass;

	/** Press to release (Deadlock AbilityCastDelay 0.25). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	float CastDelay = 0.25f;

	/** Gap between the two rooks (Deadlock DeployGap 75 u). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	float DeployGap = 133.0f;

	/** Spirit damage per hit (Deadlock Damage 50). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	FScalableFloat Damage = 50.0f;

	/** Root on hit (Deadlock ImmobilizeDuration 1, +0.5 at T3). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	FScalableFloat ImmobilizeDuration = 1.0f;

	/** Wall bounces per rook, each back along the path it came (Deadlock MaxReflectCount 1, +1 at T3). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	FScalableFloat MaxReflects = 1.0f;

	/** cm/s (Deadlock projectile speed 850 u/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	FScalableFloat RookSpeed = 1511.0f;

	/** Flat distance before the fork (35 m; a guess, Deadlock's data doesn't say). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	FScalableFloat MaxTravelDistance = 2450.0f;

	/** Flat distance after the fork (Deadlock MoveDistance 12 m). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	FScalableFloat ForkDistance = 840.0f;

	/** Optional cosmetic cast animation (not stopped on end). */
	UPROPERTY(EditDefaultsOnly, Category = "Rook")
	TObjectPtr<UAnimMontage> CastMontage;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> CastDelayTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> RecastTask;

	/** Server. */
	TArray<TWeakObjectPtr<AZL_Solomon_Rook>> Rooks;

	bool bEnded = false;

	UFUNCTION()
	void OnCastDelayFinished();

	UFUNCTION()
	void OnRecastPressed(float TimeWaited);

	UFUNCTION()
	void OnRookDestroyed(AActor* DestroyedActor);

	/** Server. */
	void SpawnRooks();
};
