// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_Kit_Projectile.h"
#include "ZL_Kit_ProjectileAbility.generated.h"

class AProjectile;
class AZeroLockCharacter;
class UAnimMontage;

/**
 * Hero kit: throws predicted AZL_Kit_Projectiles at the crosshair. Count per volley in a flat fan of FanAngle, VolleyCount
 * volleys VolleyInterval apart (each next interval × VolleyIntervalScale). Optional self payload and self move first
 * (leap, then throw after FireDelay). The ability ends once every projectile has spawned; they outlive it.
 */
UCLASS()
class ZEROLOCK_API UZL_Kit_ProjectileAbility : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Kit_ProjectileAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	TSubclassOf<AZL_Kit_Projectile> ProjectileClass;

	/** Falls back to ProjectileClass. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	TSubclassOf<AZL_Kit_Projectile> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	float SpawnForwardOffset = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile", meta = (ClampMin = 1))
	int32 Count = 1;

	/** Yaw between the outermost projectiles of a volley, degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	float FanAngle = 0.0f;

	/** Extra pitch up, degrees (lobbed grenades). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	float PitchOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile", meta = (ClampMin = 1))
	int32 VolleyCount = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	float VolleyInterval = 0.25f;

	/** Each interval is the last × this (0.8 = faster and faster). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	float VolleyIntervalScale = 1.0f;

	/** Seconds from the press to the first volley (after a leap). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Projectile")
	float FireDelay = 0.0f;

	/** What each projectile does when it lands. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit")
	FZL_KitProjectileHit Hit;

	/** Each volley's explosion radius grows by this (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Hit")
	float AreaRadiusPerVolley = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FScalableFloat PowerScale = 1.0f;

	/** On the caster at the press (server). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	FZL_KitPayload SelfPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	float MoveForwardSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	float MoveUpSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	float MoveDuration = 0.25f;

	/** Stops the caster when the move ends (hang in the air while throwing; pair with Zerolock.Status.Floating). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|Self")
	bool bStopAfterMove = false;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	TObjectPtr<UAnimMontage> CastMontage;

	/** Cue on the caster at the press (predicted for the owner). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|FX")
	FZL_KitCue CastCue;

	/** Server: a projectile spawned and was armed. */
	virtual void OnProjectileArmed(AZL_Kit_Projectile* Projectile) {}

	/** What the server arms each projectile with. Default: Hit, radius grown per volley. */
	virtual FZL_KitProjectileHit GetHitForVolley(int32 Volley) const;

private:
	void FireVolley();
	void StartSpawnTask(const FVector& Location, const FRotator& Rotation);
	void OnSpawnFinished();
	void GetAim(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const;

	UFUNCTION()
	void OnProjectileSpawned(AProjectile* Spawned);

	UFUNCTION()
	void OnProjectileFailed(AProjectile* Spawned);

	UFUNCTION()
	void OnMoveFinished();

	int32 PendingSpawns = 0;
	int32 VolleysFired = 0;
	/** Server: volley of each spawned projectile, in spawn order. */
	int32 ArmedCount = 0;
	float NextInterval = 0.0f;
	FTimerHandle VolleyTimer;
	FTimerHandle MoveTimer;
	bool bEnding = false;
};
