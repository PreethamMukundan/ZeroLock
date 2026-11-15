// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "HeroKit/ZL_Kit_Ability.h"
#include "HeroKit/ZL_Kit_Passive.h"
#include "HeroKit/ZL_Kit_TargetAbility.h"
#include "GameplayEffect.h"
#include "HeroKit/ZL_KitCue.h"
#include "HeroKit/ZL_Kit_Zone.h"
#include "ZL_HeroPassives.generated.h"

/*
 * Small hero-specific abilities from the hero sheet (Docs/Claude/HeroKit.md "Heroes"). Each builds on a kit class; their
 * numbers live on the hero's ability BPs.
 */

/** Yatagarasu "Sunlit Vigil": alive VigilTime seconds in a row → YokaiPayload (stats for its StatDuration), then the count restarts after it. */
UCLASS()
class ZEROLOCK_API UZL_Yatagarasu_SunlitVigil : public UZL_Kit_Passive
{
	GENERATED_BODY()

protected:
	virtual void OnPassiveStarted() override;
	virtual void OnPassiveEnded() override;

	UPROPERTY(EditDefaultsOnly, Category = "Yatagarasu")
	float VigilTime = 90.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Yatagarasu")
	FZL_KitPayload YokaiPayload;

private:
	void StartVigil();
	void Transform();
	FTimerHandle VigilTimer;
};

/** JA594 "Gateway": first press leaves a gate at your feet (no cooldown); second press within GateLifetime teleports you back to it. */
UCLASS()
class ZEROLOCK_API UZL_JA594_Gateway : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_JA594_Gateway();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Gateway")
	float GateLifetime = 12.0f;

	/** Marker disc look. */
	UPROPERTY(EditDefaultsOnly, Category = "Gateway")
	FLinearColor GateColor = FLinearColor(0.6f, 0.1f, 1.0f, 1.0f);

private:
	bool HasGate() const;

	FVector GateLocation = FVector::ZeroVector;
	double GateExpireTime = 0.0;
	TWeakObjectPtr<AZL_Kit_Zone> GateMarker;
};

/**
 * JA594 "Unholy Machine": KillsToAwaken kills without dying → awakened for AwakenDuration: every time he takes a hit
 * (at most once per ExplodeCooldown) he explodes (ExplosionPayload on enemies within ExplosionRadius) and loses
 * SelfHealthCostPercent of his max health (never below 1).
 */
UCLASS()
class ZEROLOCK_API UZL_JA594_UnholyMachine : public UZL_Kit_Passive
{
	GENERATED_BODY()

protected:
	virtual void OnPassiveStarted() override;
	virtual void OnPassiveEnded() override;
	virtual void OnKill(AActor* Victim) override;

	UPROPERTY(EditDefaultsOnly, Category = "JA594")
	int32 KillsToAwaken = 3;

	UPROPERTY(EditDefaultsOnly, Category = "JA594")
	float AwakenDuration = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "JA594")
	float ExplosionRadius = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "JA594")
	FZL_KitPayload ExplosionPayload;

	UPROPERTY(EditDefaultsOnly, Category = "JA594")
	float SelfHealthCostPercent = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "JA594")
	float ExplodeCooldown = 1.0f;

private:
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void Explode();

	int32 Streak = 0;
	double AwakenUntil = 0.0;
	double NextExplodeTime = 0.0;
	bool bApplyingCost = false;
	FDelegateHandle HealthHandle;
};

/**
 * Genghis Khan "Cruel Warmonger": while the enemy team has at least DifferenceToStart more kills than his, each kill of
 * difference past that adds one stack of StatsPerStack (up to MaxStacks; the sheet has no cap, which is a balance risk).
 */
UCLASS()
class ZEROLOCK_API UZL_Genghis_CruelWarmonger : public UZL_Kit_Passive
{
	GENERATED_BODY()

protected:
	virtual void OnPassiveStarted() override;
	virtual void OnPassiveEnded() override;

	UPROPERTY(EditDefaultsOnly, Category = "Genghis")
	int32 DifferenceToStart = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Genghis")
	TArray<FZL_ItemStatValue> StatsPerStack;

	UPROPERTY(EditDefaultsOnly, Category = "Genghis")
	int32 MaxStacks = 10;

private:
	void Check();

	FTimerHandle CheckTimer;
	FActiveGameplayEffectHandle BuffHandle;
	int32 AppliedStacks = 0;
};

/** Ashura "Monarch": KillsForMonarch kills in one life sets the battlefield ablaze: BlazePayload on every enemy hero. */
UCLASS()
class ZEROLOCK_API UZL_Ashura_Monarch : public UZL_Kit_Passive
{
	GENERATED_BODY()

protected:
	virtual void OnPassiveStarted() override { Kills = 0; }
	virtual void OnKill(AActor* Victim) override;

	UPROPERTY(EditDefaultsOnly, Category = "Ashura")
	int32 KillsForMonarch = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Ashura")
	FZL_KitPayload BlazePayload;

private:
	int32 Kills = 0;
};

/**
 * Ryu "The Beast Awakens": using all four heavenly beast abilities (Secondary, Ability 1, Ability 2, Ultimate) each within
 * ChainWindow of the last, without dying, heals HealMaxHealthPercent of his max health.
 */
UCLASS()
class ZEROLOCK_API UZL_Ryu_BeastAwakens : public UZL_Kit_Passive
{
	GENERATED_BODY()

protected:
	virtual void OnPassiveStarted() override;
	virtual void OnPassiveEnded() override;

	UPROPERTY(EditDefaultsOnly, Category = "Ryu")
	float ChainWindow = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ryu")
	float HealMaxHealthPercent = 50.0f;

private:
	void OnAbilityCommitted(UGameplayAbility* Ability);

	TSet<int32> Used;
	double LastUse = 0.0;
	FDelegateHandle CommittedHandle;
};

/**
 * Kronos "Looping Past": every LoopInterval seconds, if he has less health than LookBack seconds ago, it goes back to that.
 */
UCLASS()
class ZEROLOCK_API UZL_Kronos_LoopingPast : public UZL_Kit_Passive
{
	GENERATED_BODY()

protected:
	virtual void OnPassiveStarted() override;
	virtual void OnPassiveEnded() override;

	UPROPERTY(EditDefaultsOnly, Category = "Kronos")
	float LoopInterval = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kronos")
	float LookBack = 3.0f;

private:
	void Sample();

	TArray<TPair<double, float>> History;
	double NextLoop = 0.0;
	FTimerHandle SampleTimer;
};

/**
 * Mephisto "Spatial Rift": aim at any hero (cone, TargetTeam Any) and confirm to mark them; press again within RiftWindow to
 * teleport them to where you aim (up to RiftRange, onto the floor). Commits on the mark.
 */
UCLASS()
class ZEROLOCK_API UZL_Mephisto_SpatialRift : public UZL_Kit_TargetAbility
{
	GENERATED_BODY()

public:
	UZL_Mephisto_SpatialRift();

protected:
	virtual void ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data) override;

	UPROPERTY(EditDefaultsOnly, Category = "Rift")
	float RiftWindow = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rift")
	float RiftRange = 2500.0f;

private:
	UFUNCTION()
	void OnRiftPressed(float TimeWaited);

	UFUNCTION()
	void OnRiftExpired();

	TWeakObjectPtr<AZeroLockCharacter> Marked;
};

/**
 * Tempest "Wind Rider": hold jump while falling to glide (Mover Float: slow fall, capped air speed, fades over its
 * FloatFadeDuration). Runs on the owning machine only: the float rides in the predicted input.
 */
UCLASS()
class ZEROLOCK_API UZL_Tempest_WindRider : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Tempest_WindRider();

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Gliding starts only when falling faster than this (cm/s), so a jump's rise isn't slowed. */
	UPROPERTY(EditDefaultsOnly, Category = "Tempest")
	float MinFallSpeed = 50.0f;

private:
	void Update();

	FTimerHandle UpdateTimer;
	bool bGliding = false;
};

/** Kai Koa's frost on an enemy: one stack per counted hit. Every new stack restarts the timer; when it runs out all stacks
 *  go. The passive sets the duration on each spec (FrostDecayTime) and grants its DebuffStackTag (Zerolock.DebuffStack.Frost),
 *  which puts it on the HUD (GAS/ZL_DebuffStacks.h). */
UCLASS()
class ZEROLOCK_API UZL_GE_KaiKoa_Frost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UZL_GE_KaiKoa_Frost();

	static int32 GetStacks(UAbilitySystemComponent* TargetASC);
};

/**
 * Kai Koa "Frozen Fist": his melee (FrostTriggers, default light + heavy melee) adds StacksPerHit frost stacks to the enemy
 * (StackCue on each). All stacks fade FrostDecayTime seconds after the last new one. At StacksToFreeze: FreezePayload
 * (stun + root + ice cue) and the stacks clear. A frozen enemy gets no new stacks until the freeze ends.
 */
UCLASS()
class ZEROLOCK_API UZL_KaiKoa_FrozenFist : public UZL_Kit_Passive
{
	GENERATED_BODY()

protected:
	virtual void OnPassiveStarted() override;
	virtual void OnPassiveEnded() override;

	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa")
	TArray<EZL_ProcTrigger> FrostTriggers = { EZL_ProcTrigger::MeleeHit, EZL_ProcTrigger::HeavyMeleeHit };

	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa")
	int32 StacksPerHit = 1;

	/** HUD debuff stack tag (a Zerolock.DebuffStack child). Empty = Zerolock.DebuffStack.Frost. */
	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa", meta = (Categories = "Zerolock.DebuffStack"))
	FGameplayTag DebuffStackTag;

	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa")
	int32 StacksToFreeze = 3;

	/** Seconds without a new stack before all of them fade. */
	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa")
	float FrostDecayTime = 3.0f;

	/** Seconds before the same enemy can gain frost again (a multi-pellet or multi-hit strike counts once). */
	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa")
	float StackCooldownPerTarget = 0.2f;

	/** On the enemy at StacksToFreeze (stun/root, damage, HitCue). Its StatusDuration is the freeze length. */
	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa")
	FZL_KitPayload FreezePayload;

	/** Played on the enemy for each stack added below the freeze. */
	UPROPERTY(EditDefaultsOnly, Category = "Kai Koa")
	FZL_KitCue StackCue;

private:
	void OnFrostHit(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource);

	FDelegateHandle FrostHandle;
	TMap<TWeakObjectPtr<AZeroLockCharacter>, double> LastStackTime;
	TMap<TWeakObjectPtr<AZeroLockCharacter>, double> FrozenUntil;
};
