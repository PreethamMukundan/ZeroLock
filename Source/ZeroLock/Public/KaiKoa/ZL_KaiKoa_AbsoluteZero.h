// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_KaiKoa_AbsoluteZero.generated.h"

class AZeroLockCharacter;
class UAnimMontage;

/**
 * Kai Koa's ultimate, Absolute Zero. Plays CastMontage (optional) and CastCue, then after FreezeDelay every enemy within
 * Radius of Kai (no line of sight needed) takes SpiritDamage and is stunned and rooted for FreezeDuration, with a status
 * timer and FrozenCue for the whole freeze. Server only; the client never replicates its end (see TsunamiFist).
 */
UCLASS()
class ZEROLOCK_API UZL_KaiKoa_AbsoluteZero : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_KaiKoa_AbsoluteZero();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Seconds from the press to the freeze. */
	UPROPERTY(EditDefaultsOnly, Category = "Absolute Zero")
	float FreezeDelay = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Absolute Zero")
	FScalableFloat Radius = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Absolute Zero")
	FScalableFloat SpiritDamage = 90.0f;

	/** Stun + root length. */
	UPROPERTY(EditDefaultsOnly, Category = "Absolute Zero")
	FScalableFloat FreezeDuration = 3.0f;

	/** Played on the ASC at the press (optional; the hero has no cast animation yet). */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<UAnimMontage> CastMontage;

	/** On Kai at the press (owner predicted + server). Radius = Radius. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue CastCue;

	/** The freeze wave around Kai (server). Radius = Radius. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue StrikeCue;

	/** On each frozen enemy (server). Its Duration should match FreezeDuration. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue FrozenCue;

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDrawDebug = false;

private:
	UFUNCTION()
	void OnFreezeDelayFinished();

	UFUNCTION()
	void OnFinished();

	void Freeze(AZeroLockCharacter* Hero);
};
