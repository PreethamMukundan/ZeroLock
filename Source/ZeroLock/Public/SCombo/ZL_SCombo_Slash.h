// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_SCombo_Slash.generated.h"

class AProjectile;
class AZeroLockCharacter;
class UAnimMontage;

/**
 * SCombo's first ability. With an enemy in the cone in front of the hero: a slash that deals weapon damage to everyone in it.
 * With nobody there: fires AZL_SCombo_Projectile along the camera, spirit damage. The owner's machine picks one and tells the
 * server with a replicated event, so a remote server's spawn task is only started for a shot the client really fires.
 */
UCLASS()
class ZEROLOCK_API UZL_SCombo_Slash : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_SCombo_Slash();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** How far the slash reaches (cm), scaled by the hero's ability range. */
	UPROPERTY(EditDefaultsOnly, Category = "Slash")
	FScalableFloat SlashRange = 500.0f;

	/** Half angle of the cone, in degrees, measured flat so enemies above or below still count. */
	UPROPERTY(EditDefaultsOnly, Category = "Slash", meta = (ClampMin = 0, ClampMax = 180))
	float SlashHalfAngle = 45.0f;

	/** Weapon damage to each enemy in the cone. */
	UPROPERTY(EditDefaultsOnly, Category = "Slash")
	FScalableFloat SlashDamage = 80.0f;

	/** Added on the server's re-check of a remote client's slash, for what moved since the client swung. */
	UPROPERTY(EditDefaultsOnly, Category = "Slash")
	float ServerRangeSlack = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Slash")
	float ServerAngleSlack = 15.0f;

	/** Played on the slash. Optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Slash")
	TObjectPtr<UAnimMontage> SlashMontage;

	/** Fake (client predicted) projectile class. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AProjectile> ProjectileClass;

	/** Authoritative server projectile class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<AProjectile> ServerProjectileClass;

	/** Spirit damage of a direct projectile hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	FScalableFloat ProjectileDamage = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float SpawnForwardOffset = 100.0f;

	/** Played on the shot. Optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TObjectPtr<UAnimMontage> ProjectileMontage;

	/** Draws the cone and a sphere on each enemy in it, on the server. On until the ability has a real effect. */
	UPROPERTY(EditDefaultsOnly, Category = "Slash")
	bool bDrawSlashDebug = true;

private:
	void FindSlashTargets(AZeroLockCharacter* Hero, float RangeSlack, float AngleSlack, TArray<AZeroLockCharacter*>& OutVillans) const;
	void DoSlash();
	void DoProjectile();
	void FinishSlashAbility(bool bWasCancelled);
	void StopWaitingForClient();

	void OnClientChoseSlash();
	void OnClientChoseProjectile();

	UFUNCTION()
	void OnProjectileSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile);

	FDelegateHandle SlashEventHandle;
	FDelegateHandle ProjectileEventHandle;
};
