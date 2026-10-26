// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "ZL_WeaponStats.generated.h"

class AProjectile;

/**
 * Per-hero primary weapon shape (AZeroLockCharacter::WeaponStats; the Deadlock importer fills it from the hero's weapon).
 * Damage per bullet, fire rate, clip and reload time are attributes (BulletDamage, FireRate, MaxAmmo, ReloadTime) so
 * items can change them; this is what stays fixed per hero.
 */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_HeroWeaponStats
{
	GENERATED_BODY()

	/** Bone or socket on the hero mesh that shots leave from (Deadlock's primary muzzle attachment's bone). None = the
	 * fire ability's MuzzleSocketName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocket = NAME_None;

	/** This hero's bullet (speed, gravity, range, tracer), used for both the shooter's predicted copy and the server's.
	 * None = the fire ability's ProjectileClass / ServerProjectileClass. The importer makes BP_<Hero>_Projectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AProjectile> ProjectileClass;

	/** When set, UZL_GA_WeaponFire takes its fire mode from Pellets/BurstShots below instead of its own settings:
	 * Pellets > 1 = Shotgun, else BurstShots > 1 = Burst, else Single. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bOverrideFireMode = false;

	/** Projectiles per shot (Deadlock m_iBullets). One ammo per shot however many pellets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1", EditCondition = "bOverrideFireMode"))
	int32 Pellets = 1;

	/** Half-angle of the pellet cone in degrees (Shotgun). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0", EditCondition = "bOverrideFireMode"))
	float PelletSpread = 6.0f;

	/** Shots per trigger pull (Deadlock m_iBurstShotCount); each costs one ammo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1", EditCondition = "bOverrideFireMode"))
	int32 BurstShots = 1;

	/** Seconds between burst shots (Deadlock m_flIntraBurstCycleTime). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.01", EditCondition = "bOverrideFireMode"))
	float BurstInterval = 0.07f;

	/** Damage falloff by distance from where the shot was fired (cm). Full damage up to FalloffStartRange, then linear
	 * down to FalloffEndScale at FalloffEndRange and beyond. FalloffEndRange <= FalloffStartRange = no falloff. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Falloff", meta = (ClampMin = "0"))
	float FalloffStartRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Falloff", meta = (ClampMin = "0"))
	float FalloffEndRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Falloff", meta = (ClampMin = "0", ClampMax = "1"))
	float FalloffEndScale = 1.0f;

	/** Damage multiplier for a shot fired from Distance cm away. */
	float GetFalloffScale(float Distance) const
	{
		if (FalloffEndRange <= FalloffStartRange)
		{
			return 1.0f;
		}
		const float Alpha = FMath::Clamp((Distance - FalloffStartRange) / (FalloffEndRange - FalloffStartRange), 0.0f, 1.0f);
		return FMath::Lerp(1.0f, FalloffEndScale, Alpha);
	}
};
