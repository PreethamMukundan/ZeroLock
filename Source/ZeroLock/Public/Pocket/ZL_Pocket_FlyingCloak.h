// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Pocket_FlyingCloak.generated.h"

class AProjectile;
class AZL_Pocket_CloakProjectile;
class AZeroLockCharacter;
class UZL_WaitDelay_Task;
class UAbilityTask_WaitInputPress;

/**
 * Fires a predicted cloak projectile that deals spirit damage to enemies it passes through.
 * Pressing the ability again within the teleport window teleports Pocket to the projectile.
 */
UCLASS()
class ZEROLOCK_API UZL_Pocket_FlyingCloak : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Pocket_FlyingCloak();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Fake (client predicted) projectile class. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Pocket_CloakProjectile> ProjectileClass;

	/** Authoritative server projectile class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Pocket_CloakProjectile> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float SpawnForwardOffset = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat SpiritDamage;

	/** How long the player has to press the ability again to teleport to the projectile. */
	UPROPERTY(EditDefaultsOnly, Category = "Timing")
	FScalableFloat TeleportWindow = 2.0f;

	// --- Active Tasks ---
	UPROPERTY()
	UZL_WaitDelay_Task* WaitTimeTask;

	UPROPERTY()
	UAbilityTask_WaitInputPress* InputPressedTask;

	UPROPERTY()
	TWeakObjectPtr<AProjectile> CloakProjectile;

	/** Last known projectile location, kept in case the projectile is destroyed (e.g. after hitting a wall) before the teleport. */
	FVector LastProjectileLocation = FVector::ZeroVector;
	bool bHasProjectileLocation = false;

	// --- Callbacks ---
	UFUNCTION()
	void OnProjectileSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnCloakProjectileDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnInputPressed(float TimeWaited);

	UFUNCTION()
	void OnTeleportWindowFinished();

	void GetProjectileSpawnTransform(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const;
	bool GetTeleportLocation(FVector& OutLocation) const;
	void CleanupProjectile();
};
