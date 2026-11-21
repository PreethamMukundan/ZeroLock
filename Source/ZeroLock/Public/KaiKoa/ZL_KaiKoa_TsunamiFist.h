// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_KaiKoa_TsunamiFist.generated.h"

class AZeroLockCharacter;

/**
 * Kai Koa's Tsunami Fist. A short lunge along the aim (LungeSpeed for LungeDuration, flat, predicted through
 * RequestSafeAbilityMove on the owner), then after HitDelay a wave punch in a flat cone in front of him (HitRange,
 * HitHalfAngle): each enemy takes SpiritDamage and is knocked away (KnockbackSpeed + KnockUpSpeed for KnockbackDuration,
 * Forced victim event). The hit is server only; the client never replicates its end, so a fast client can't cancel the
 * server's instance before the punch lands (same as UZL_BasePlayAnimation_AndDo).
 */
UCLASS()
class ZEROLOCK_API UZL_KaiKoa_TsunamiFist : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_KaiKoa_TsunamiFist();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lunge")
	float LungeSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lunge")
	float LungeDuration = 0.15f;

	/** Seconds after the press until the punch hits. */
	UPROPERTY(EditDefaultsOnly, Category = "Punch")
	float HitDelay = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Punch")
	FScalableFloat HitRange = 450.0f;

	/** Half angle of the cone, degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Punch")
	float HitHalfAngle = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Punch")
	FScalableFloat SpiritDamage = 70.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Punch")
	float KnockbackSpeed = 1300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Punch")
	float KnockUpSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Punch")
	float KnockbackDuration = 0.2f;

	/** On Kai at the press (owner predicted + server). */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue CastCue;

	/** The wave, at the punch (server). Radius = HitRange. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue WaveCue;

	/** On each enemy hit (server). */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue HitHeroCue;

	/** Draws the punch cone on the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDrawDebug = false;

private:
	UFUNCTION()
	void OnHitDelayFinished();

	UFUNCTION()
	void OnFinished();

	void Punch(AZeroLockCharacter* Hero);
	void FindTargets(AZeroLockCharacter* Hero, const FVector& Origin, const FVector& Forward, float Range, TArray<AZeroLockCharacter*>& OutVillans) const;

	/** Flat aim at the press: the lunge and the punch both use it. */
	FVector PunchDirection = FVector::ForwardVector;
};
