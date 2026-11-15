// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitTypes.h"
#include "ZL_Kit_Ability.generated.h"

class UAnimMontage;
class AZeroLockCharacter;

/**
 * Hero kit: the general "press and it happens" ability. In order, all optional:
 *   SelfPayload on the caster (buffs, statuses, timed tags) at the press;
 *   a predicted self move (leap, lunge) along the aim;
 *   after HitDelay, PulseCount area hits every PulseInterval (EnemyPayload / AllyPayload);
 *   for ActiveDuration, every hit the caster lands with HitPayloadTrigger also gets WhileActiveHitPayload.
 * The ability lasts max(ActiveDuration, the last pulse) and holds ActivationOwnedTags for that long.
 * Configure everything in a BP child (Docs/Claude/HeroKit.md).
 */
UCLASS()
class ZEROLOCK_API UZL_Kit_Ability : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Kit_Ability();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Multiplies damage, heal, barrier and DoT of every payload. Use the level curve for upgrades. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FScalableFloat PowerScale = 1.0f;

	/** On the caster at the press (server). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	FZL_KitPayload SelfPayload;

	/** Seconds the ability stays active (ActivationOwnedTags, WhileActiveHitPayload, progress bar). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	float ActiveDuration = 0.0f;

	/** Shows the HUD progress bar for ActiveDuration. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	bool bShowProgressBar = true;

	/** Re-pressing while active ends it early. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	bool bEndOnRepress = false;

	/** Self move: speed along the aim (flat unless bMoveAlongPitch), cm/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Move")
	float MoveForwardSpeed = 0.0f;

	/** Self move: upward speed, cm/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Move")
	float MoveUpSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Move")
	float MoveDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Move")
	bool bMoveAlongPitch = false;

	/** Stop dead when the move ends instead of keeping its speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Move")
	bool bStopAfterMove = false;

	/** No area hits when false. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit")
	bool bHasAreaHit = true;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	FZL_KitArea Area;

	/** Area origin: ForwardOffset cm ahead of the caster (flat). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	float ForwardOffset = 0.0f;

	/** Seconds from the press to the first pulse. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	float HitDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit", ClampMin = 1))
	int32 PulseCount = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	float PulseInterval = 0.5f;

	/** Each pulse picks its spot at random within RandomStrikeSpread of the origin and hits Area around it (lightning, spikes). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	float RandomStrikeSpread = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	FZL_KitPayload EnemyPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	FZL_KitPayload AllyPayload;

	/** Whether AllyPayload also lands on the caster. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit", meta = (EditCondition = "bHasAreaHit"))
	bool bAllyPayloadOnSelf = true;

	/** While active: extra payload on whoever the caster damages (see WhileActiveTriggers). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|While Active")
	FZL_KitPayload WhileActiveHitPayload;

	/** Percent chance per hit (0 = always). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|While Active")
	float WhileActiveHitChance = 0.0f;

	/** Which hits count. Empty = bullets and melee. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|While Active")
	TArray<EZL_ProcTrigger> WhileActiveTriggers;

	/** Optional, cosmetic. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	TObjectPtr<UAnimMontage> CastMontage;

	/** Cue on the caster at the press (predicted for the owner; Duration > 0 = a buff loop that follows them). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|FX")
	FZL_KitCue CastCue;

	/** Cue at each pulse's origin, facing the aim; radius = the area's range. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|FX")
	FZL_KitCue PulseCue;

	/** Draws each pulse's area on the server. On until the ability has VFX. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	bool bDrawDebug = true;

	/** Server, per pulse. Override for hero-specific rules; the default finds heroes in Area and applies the payloads. */
	virtual void DoPulse(int32 PulseIndex);

	/** Server, for each enemy a pulse hits, before EnemyPayload. Return false to skip the payload. */
	virtual bool OnPulseHitEnemy(AZeroLockCharacter* Villan, int32 PulseIndex) { return true; }

	/** Server, after the press's self payload. */
	virtual void OnKitActivated() {}

	/** Server, while active: a hit the caster landed (after the while-active payload). */
	virtual void OnWhileActiveHit(AZeroLockCharacter* Villan, float Damage, EZL_ProcTrigger Trigger) {}

	FVector GetAreaOrigin(const AZeroLockCharacter* Hero) const;
	float GetPower() const { return PowerScale.GetValueAtLevel(GetAbilityLevel()); }
	AZeroLockCharacter* GetHero() const;

	/** Origin and aim of the current activation, fixed at the press (server). */
	FVector CastOrigin = FVector::ZeroVector;
	FVector CastDirection = FVector::ForwardVector;

private:
	void StartPulses();
	void PulseTick();
	void OnDamageDealt(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource);

	UFUNCTION()
	void OnDurationFinished();

	UFUNCTION()
	void OnRepressed(float TimeWaited);

	UFUNCTION()
	void OnMoveFinished();

	int32 PulsesDone = 0;
	FTimerHandle PulseTimer;
	FTimerHandle MoveTimer;
	FDelegateHandle DamageDealtHandle;
	bool bEnding = false;
};
