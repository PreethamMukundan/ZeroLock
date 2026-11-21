// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Tempest_AlleyOop.generated.h"

class AProjectile;
class AZL_Tempest_WindBall;

/**
 * Tempest's secondary, Alley-oop. 2 charges. Throws one predicted wind ball (AZL_Tempest_WindBall) at the crosshair;
 * it keeps turning toward the crosshair, so the player steers it, and bursts on the first enemy or wall for BurstDamage
 * in BurstRadius. The remote server starts its spawn task at activation and ends itself when it reports; the client's
 * end isn't replicated (it would cancel the server's task before the spawn data arrives).
 */
UCLASS()
class ZEROLOCK_API UZL_Tempest_AlleyOop : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Tempest_AlleyOop();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Alley-oop")
	TSubclassOf<AZL_Tempest_WindBall> ProjectileClass;

	/** Falls back to ProjectileClass. */
	UPROPERTY(EditDefaultsOnly, Category = "Alley-oop")
	TSubclassOf<AZL_Tempest_WindBall> ServerProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Alley-oop")
	float SpawnForwardOffset = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Alley-oop")
	FScalableFloat BurstDamage = 75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Alley-oop")
	FScalableFloat BurstRadius = 300.0f;

private:
	UFUNCTION()
	void OnBallSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnBallFailed(AProjectile* SpawnedProjectile);

	void Finish();
};
