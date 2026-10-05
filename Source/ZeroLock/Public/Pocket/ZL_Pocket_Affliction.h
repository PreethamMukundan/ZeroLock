// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BasePlayAnimation_AndDo.h"
#include "ZL_Pocket_Affliction.generated.h"

class AZeroLockCharacter;
class UGameplayEffect;

/**
 * Pocket plays MontageToPlay. At the montage's trigger event (or when it finishes, if it never sends one) every enemy
 * within Radius is afflicted: AfflictionEffect deals DamagePerTick spirit damage every TickInterval for Duration seconds.
 * From HealBlockMinLevel on it also grants Zerolock.Status.HealBlocked, which stops all healing for that time.
 * Each victim gets an "Afflicted" progress bar for the duration through UZL_StatusTimerAbility.
 */
UCLASS()
class ZEROLOCK_API UZL_Pocket_Affliction : public UZL_BasePlayAnimation_AndDo
{
	GENERATED_BODY()

public:
	UZL_Pocket_Affliction();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnAnimationPointTrigger() override;
	virtual void OnAnimationCompleted() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat Radius = 800.0f;

	/** How long the affliction lasts on each enemy. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat Duration = 6.0f;

	/** Spirit damage dealt on application and then every TickInterval. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat DamagePerTick = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	float TickInterval = 1.0f;

	/** From this ability level on, afflicted enemies can't be healed. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	int32 HealBlockMinLevel = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> AfflictionEffect;

	/** Draws the affliction sphere (green if it hit anyone) on the server and the owning client. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	bool bDrawDebugAffliction = false;

	/** The montage can both send the trigger event and complete, so only the first one afflicts. */
	bool bHasAfflicted = false;

	void AfflictNearbyEnemies();
	void FindTargets(AZeroLockCharacter* Hero, TSet<AZeroLockCharacter*>& OutVillans) const;
	void ApplyAffliction(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan) const;
};
