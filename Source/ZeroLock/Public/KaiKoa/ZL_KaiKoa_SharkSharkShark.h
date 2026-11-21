// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_KaiKoa_SharkSharkShark.generated.h"

class AProjectile;
class AZL_KaiKoa_SharkProjectile;
class AZeroLockCharacter;

/**
 * Kai Koa's secondary, Shark Shark Shark. Throws NumSharks predicted sharks (AZL_KaiKoa_SharkProjectile) at the
 * crosshair in a flat fan FanAngle wide. Each shark bites every enemy it passes once for SpiritDamage.
 * The owner spawns them all at once; the remote server starts one spawn task per shark at activation (each consumes one
 * packet of the client's spawn data) and ends once they've all reported.
 */
UCLASS()
class ZEROLOCK_API UZL_KaiKoa_SharkSharkShark : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_KaiKoa_SharkSharkShark();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Fake (client predicted) shark. */
	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	TSubclassOf<AZL_KaiKoa_SharkProjectile> ProjectileClass;

	/** Server shark. Falls back to ProjectileClass. */
	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	TSubclassOf<AZL_KaiKoa_SharkProjectile> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shark", meta = (ClampMin = "1"))
	int32 NumSharks = 3;

	/** Yaw between the outermost sharks, degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	float FanAngle = 24.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	float SpawnForwardOffset = 100.0f;

	/** Per shark, per enemy. */
	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	FScalableFloat SpiritDamage = 45.0f;

	/** On Kai at the throw (owner predicted + server). */
	UPROPERTY(EditDefaultsOnly, Category = "Shark")
	FZL_KitCue CastCue;

private:
	void StartSpawnTask(const FVector& Location, const FRotator& Rotation);
	void GetAim(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const;
	void OnSpawnReported();

	UFUNCTION()
	void OnSharkSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnSharkFailedToSpawn(AProjectile* SpawnedProjectile);

	/** Spawn tasks that haven't reported yet. The ability ends at zero. */
	int32 PendingSpawns = 0;
};
