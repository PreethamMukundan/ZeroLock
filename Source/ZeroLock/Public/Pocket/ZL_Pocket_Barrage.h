// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Pocket_Barrage.generated.h"

class AProjectile;
class AZL_Pocket_BarrageProjectile;
class AZeroLockCharacter;
class UGameplayEffect;
class UZL_WaitDelay_Task;

/**
 * Pocket floats (keeping his current momentum, which move input can only steer slightly) and fires a burst of predicted projectiles.
 * Trying to use another ability mid-barrage ends it and activates that ability instead.
 * Each projectile sphere-traces on impact and deals spirit damage; every impact that hits an enemy grants Pocket a
 * Barrage stack (StackEffect) which buffs the UniversalDamage attribute. Stacks are cleared when the ability is activated.
 */
UCLASS()
class ZEROLOCK_API UZL_Pocket_Barrage : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Pocket_Barrage();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Fake (client predicted) projectile class. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Pocket_BarrageProjectile> ProjectileClass;

	/** Authoritative server projectile class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Pocket_BarrageProjectile> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float SpawnForwardOffset = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (ClampMin = "1"))
	int32 NumProjectiles = 4;

	/** If true, the projectiles are spread evenly across FloatDuration (interval = duration / NumProjectiles) and FireInterval is ignored. */
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	bool bScaleFireIntervalToDuration = true;

	/** Time between projectiles when bScaleFireIntervalToDuration is off. The first one fires on activation. */
	UPROPERTY(EditDefaultsOnly, Category = "Timing", meta = (EditCondition = "!bScaleFireIntervalToDuration"))
	float FireInterval = 0.6f;

	/** How long Pocket floats (the ability's duration). Never shorter than the time it takes to fire every projectile. */
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	FScalableFloat FloatDuration = 3.0f;

	/** Pressing jump or the ability input again ends the barrage early (the cost and cooldown are already committed). */
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	bool bCanCancelEarly = true;

	/** Other abilities are blocked while floating (MovementLock is under ZeroLock.Abilities). Trying to use one ends the barrage
	 * and activates it instead, except for abilities bound to these inputs, which just stay blocked.
	 * Abilities with no input (event triggered, e.g. knockback or pull) never end it. */
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	TArray<EGASAbilityInputID> InputsThatDontCancel = { EGASAbilityInputID::Primary_Attack, EGASAbilityInputID::Reload };

	/** Mover mode used while floating. Must be a ZeroFloatMode with bPreserveMomentum enabled (its SteerAcceleration sets how much input steers). */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	FName FloatingModeName = FName("Floating");

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat SpiritDamage;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat ImpactRadius = 200.0f;

	/** Applied to Pocket for every impact that hits an enemy. Should stack (limit 4, refresh duration on apply)
	 * and grant the Barrage stack tag plus a UniversalDamage modifier. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> StackEffect;

	UPROPERTY()
	UZL_WaitDelay_Task* FloatTask;

	int32 ShotsFired = 0;

	/** Time between projectiles for the current activation (FireInterval, or scaled to the float duration). */
	float CurrentFireInterval = 0.0f;

	FDelegateHandle JumpPressedHandle;

	FDelegateHandle AbilityFailedHandle;

	/** True once the MovementLock tag was added and the floating mode was queued, so EndAbility only undoes what we did. */
	bool bIsFloating = false;

	/** Remote server only: spawn tasks still waiting on the client's projectile data. */
	int32 PendingServerSpawns = 0;

	/** Remote server only: the float timer finished, but we're still waiting on projectile data from the client. */
	bool bFinishWhenSpawnsDone = false;

	void StartFloatTimer();
	void ListenForCancel(AZeroLockCharacter* Hero);
	void CancelBarrage();
	void OnJumpPressed();
	void OnAbilityFailed(const UGameplayAbility* FailedAbility, const FGameplayTagContainer& FailureTags);
	void FireProjectile();
	void StartSpawnTask(const FVector& SpawnLocation, const FRotator& SpawnRotation);
	void FinishBarrage();
	void ClearStacks() const;
	void GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const;

	UFUNCTION()
	void OnFireIntervalFinished();

	UFUNCTION()
	void OnFloatFinished();

	UFUNCTION()
	void OnAbilityInputPressed(float TimeWaited);

	UFUNCTION()
	void OnProjectileSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile);
};
