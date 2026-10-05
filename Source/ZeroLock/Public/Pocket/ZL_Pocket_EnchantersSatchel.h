// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "MoverSimulationTypes.h"
#include "ZL_Pocket_EnchantersSatchel.generated.h"

class AZeroLockCharacter;
class AZL_Pocket_SuitcaseActor;
class UAbilityTask_WaitInputPress;
class UZL_WaitDelay_Task;

/**
 * Pocket turns into a suitcase: rooted in place with all momentum killed, and untouchable.
 * He comes out when jump or the ability input is pressed, when MaxDuration runs out, or right after a
 * Flying Cloak teleport (which carries the suitcase to the cloak). Coming out deals spirit damage to
 * every enemy within ExitDamageRadius and ends the ability.
 */
UCLASS()
class ZEROLOCK_API UZL_Pocket_EnchantersSatchel : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Pocket_EnchantersSatchel();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Longest Pocket can stay in the suitcase before he's forced out. */
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	FScalableFloat MaxDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat SpiritDamage;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat ExitDamageRadius = 400.0f;

	/** Draws the exit damage sphere (green if it hit anyone) on the server and the owning client. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	bool bDrawDebugExitDamage = false;

	/** Placeholder suitcase visual. Swap for a Blueprint subclass with the real mesh. */
	UPROPERTY(EditDefaultsOnly, Category = "Visuals")
	TSubclassOf<AZL_Pocket_SuitcaseActor> SuitcaseClass;

	/** Mode queued when Pocket comes out of the suitcase. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	FName ExitModeName = FName("Falling");

	UPROPERTY()
	UZL_WaitDelay_Task* MaxDurationTask;

	UPROPERTY()
	UAbilityTask_WaitInputPress* InputPressedTask;

	UPROPERTY()
	TWeakObjectPtr<AZL_Pocket_SuitcaseActor> SuitcaseActor;

	FDelegateHandle JumpPressedHandle;

	/** True from activation until Pocket comes out, so EndAbility only undoes (and damages) once. */
	bool bInSuitcase = false;

	void SpawnSuitcase(AZeroLockCharacter* Hero);
	void ListenForExit(AZeroLockCharacter* Hero);
	void ExitSuitcase(bool bReplicateEndAbility);
	void ApplyExitDamage(AZeroLockCharacter* Hero) const;
	void FindExitTargets(AZeroLockCharacter* Hero,TSet<AZeroLockCharacter*>& OutVillans) const;
	void DrawExitDebug(const AZeroLockCharacter* Hero, bool bHitAnyone) const;
	void OnJumpPressed();

	UFUNCTION()
	void OnAbilityInputPressed(float TimeWaited);

	UFUNCTION()
	void OnMaxDurationFinished();

	UFUNCTION()
	void OnTeleportSucceeded(const FVector& FromLocation, const FQuat& FromRotation, const FVector& ToLocation, const FQuat& ToRotation);

	UFUNCTION()
	void OnTeleportFailed(const FVector& FromLocation, const FQuat& FromRotation, const FVector& ToLocation, const FQuat& ToRotation, ETeleportFailureReason TeleportFailureReason);
};
