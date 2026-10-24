// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Viscous_GooBall_Ability.generated.h"

class AZL_Viscous_GooBall;
class UAbilityTask_WaitInputPress;
class UGameplayEffect;
class UZL_WaitDelay_Task;

/**
 * Viscous's Goo Ball (ultimate). Press: become a big bouncy goo ball for Duration (HUD progress bar via
 * UZL_WaitDelay_Task); press again to end early. Zerolock.Status.GooBall (ActivationOwnedTags, predicted) puts the
 * Mover in the GooBall mode with a sphere capsule (steer, bounce, double jump; Movement.md) and blocks the weapon;
 * other abilities still work (The Cube on self takes over the movement, and the ball comes back after).
 * Server: ResistEffect (+BulletResist / +SpiritResist percent) and an AZL_Viscous_GooBall (look + impacts:
 * ImpactDamage spirit + StunEffect to each enemy rolled through, once). Cooldown starts when the ball ends.
 */
UCLASS()
class ZEROLOCK_API UZL_Viscous_GooBall_Ability : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Viscous_GooBall_Ability();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	FScalableFloat Duration = 6.0f;

	/** Added WeaponResistance (percent). */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	FScalableFloat BulletResist = 50.0f;

	/** Added SpiritResistance (percent). */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	FScalableFloat SpiritResist = 50.0f;

	/** Spirit damage to each enemy the ball rolls into. */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	FScalableFloat ImpactDamage = 75.0f;

	/** Should grant ZeroLock.Stun (BPGE_Stun in the BP). */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	TSubclassOf<UGameplayEffect> StunEffect;

	/** Overrides StunEffect's duration when > 0. */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	FScalableFloat StunDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	TSubclassOf<UGameplayEffect> ResistEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	TSubclassOf<AZL_Viscous_GooBall> BallClass;

	UPROPERTY()
	TObjectPtr<UZL_WaitDelay_Task> DurationTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> RepressTask;

	TWeakObjectPtr<AZL_Viscous_GooBall> BallActor;
	FActiveGameplayEffectHandle ResistHandle;
	bool bBallEnded = false;

	UFUNCTION()
	void OnDurationFinished();

	UFUNCTION()
	void OnRepressed(float TimeWaited);
};
