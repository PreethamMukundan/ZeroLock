// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Xayah_ThrowAbility.generated.h"

class AProjectile;
class AZeroLockCharacter;
class AZL_Xayah_BladeProjectile;

/**
 * Base for Xayah's abilities that throw a fan of predicted blades (Double Daggers, Featherstorm). The ability ends once
 * every blade has spawned. A remote server calls ThrowFan on activation: its spawn tasks wait for the client's spawn
 * data, so the client can throw later.
 */
UCLASS(Abstract)
class ZEROLOCK_API UZL_Xayah_ThrowAbility : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Xayah_ThrowAbility();

	/** The world point under the crosshair (locally controlled only: it reads the camera). */
	static FVector TraceAimPoint(const AZeroLockCharacter* Hero);

protected:
	/** Fake (client predicted) projectile class. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Xayah_BladeProjectile> ProjectileClass;

	/** Authoritative server projectile class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AZL_Xayah_BladeProjectile> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float SpawnForwardOffset = 100.0f;

	/** Damage of each blade to each enemy it passes. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat SpiritDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (ClampMin = "1"))
	int32 BladeCount = 2;

	/** Total yaw between the outermost blades, in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (ClampMin = "0.0"))
	float FanAngle = 5.0f;

	/** Throws BladeCount blades around the crosshair. */
	void ThrowFan();

	/** Direction of one blade. Default: a flat fan of FanAngle. */
	virtual FRotator GetBladeRotation(const FRotator& AimRotation, int32 Index) const;

private:
	void GetAim(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const;
	void StartSpawnTask(const FVector& SpawnLocation, const FRotator& SpawnRotation);
	void OnBladeFinished();

	UFUNCTION()
	void OnBladeSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnBladeFailedToSpawn(AProjectile* SpawnedProjectile);

	int32 PendingSpawns = 0;
};
