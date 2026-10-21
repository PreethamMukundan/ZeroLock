// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "GAS/TargetActors/ZL_GATA_SurfaceTrace.h"
#include "ZL_Doorman_Doorway.generated.h"

class AZL_Doorman_Door;
class UAbilityTask_WaitInputPress;
class UAbilityTask_WaitTargetData;
class UAbilityTask_WaitDelay;
class AGameplayAbilityWorldReticle;

UENUM()
enum class EZL_DoorwayPhase : uint8
{
	PlacingFirst,	// aiming door 1
	WaitingSecond,	// door 1 placed; the hero moves and uses other abilities freely
	PlacingSecond,	// aiming door 2
	Open			// both doors linked, duration running
};

/**
 * Doorman's Doorway. Press: aim door 1 on a wall with AZL_GATA_SurfaceTrace, confirm to place it. Press again: aim
 * door 2 (within MaxDoorDistance of door 1, no time limit), confirm to place it and link the pair. The doors stay for DoorDuration;
 * pressing again closes them early. Cancelling the first aim costs nothing; the cooldown starts when the ability ends.
 */
UCLASS()
class ZEROLOCK_API UZL_Doorman_Doorway : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Doorman_Doorway();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Doorway")
	TSubclassOf<AZL_Doorman_Door> DoorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Doorway|Targeting")
	TSubclassOf<AZL_GATA_SurfaceTrace> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Doorway|Targeting")
	TSubclassOf<AGameplayAbilityWorldReticle> ReticleClass;

	UPROPERTY(EditDefaultsOnly, Category = "Doorway|Targeting")
	EZL_SurfaceTargetMode SurfaceMode = EZL_SurfaceTargetMode::Wall;

	UPROPERTY(EditDefaultsOnly, Category = "Doorway|Targeting")
	float MaxPlacementRange = 2000.0f;

	/** Door 2 must be within this distance of door 1. */
	UPROPERTY(EditDefaultsOnly, Category = "Doorway")
	FScalableFloat MaxDoorDistance = 2500.0f;

	/** Seconds the linked doors stay open. */
	UPROPERTY(EditDefaultsOnly, Category = "Doorway")
	FScalableFloat DoorDuration = 8.0f;

	/** Stand the door on the floor below the aimed point. No floor within twice the door height = can't place. */
	UPROPERTY(EditDefaultsOnly, Category = "Doorway")
	bool bSnapDoorToFloor = true;

	/** The door doesn't clip: free space over the frame + 60 cm to step out, flat wall behind it, floor under both jambs. */
	UPROPERTY(EditDefaultsOnly, Category = "Doorway|Placement")
	FZL_PlacementClearance DoorClearance;

	UPROPERTY(EditDefaultsOnly, Category = "Doorway|Debug")
	bool bDrawTargetingDebug = false;

	EZL_DoorwayPhase Phase = EZL_DoorwayPhase::PlacingFirst;
	bool bCleanedUp = false;

	UPROPERTY()
	TObjectPtr<AZL_Doorman_Door> FirstDoor;

	UPROPERTY()
	TObjectPtr<AZL_Doorman_Door> SecondDoor;

	/** Door 1 location as placed by this machine (the client doesn't wait for the replicated door). */
	FVector FirstDoorLocation = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitTargetData> TargetTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> InputPressTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> TimerTask;

	void StartTargeting();
	void StopTargeting();
	void WaitForPress();
	void StartTimer(float Duration);
	void StopTimer();

	/**
	 * Door transform from a surface hit: X out of the wall, Z up, optionally standing on the floor. False if the door
	 * can't go there (no floor, DoorClearance fails); OutTransform is still set when a transform exists, for the
	 * targeting preview.
	 */
	bool GetDoorTransform(const FHitResult& Hit, FTransform& OutTransform) const;
	AZL_Doorman_Door* SpawnDoor(const FTransform& DoorTransform) const;

	UFUNCTION()
	void OnTargetConfirmed(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void OnTargetCancelled(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void OnInputPressed(float TimeWaited);

	UFUNCTION()
	void OnTimerFinished();
};
