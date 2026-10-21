// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Doorman_LuggageCart.generated.h"

class AProjectile;
class AZL_Projectile_Doorman_LuggageCart;
class UAnimMontage;
class UGameplayEffect;

/**
 * Doorman sends out a Luggage Cart (predicted projectile AZL_Projectile_Doorman_LuggageCart) along his flat aim
 * direction. The cart outlives the ability: spirit damage + drag on enemies it touches, stun if it slams them into a wall.
 */
UCLASS()
class ZEROLOCK_API UZL_Doorman_LuggageCart : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Doorman_LuggageCart();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Fake (client predicted) cart class. */
	UPROPERTY(EditDefaultsOnly, Category = "Cart")
	TSubclassOf<AZL_Projectile_Doorman_LuggageCart> ProjectileClass;

	/** Authoritative server cart class. Falls back to ProjectileClass if not set. */
	UPROPERTY(EditDefaultsOnly, Category = "Cart")
	TSubclassOf<AZL_Projectile_Doorman_LuggageCart> ServerProjectileClass;

	/** Spawn point in front of Doorman. */
	UPROPERTY(EditDefaultsOnly, Category = "Cart")
	float SpawnForwardOffset = 120.0f;

	/** Spirit damage to each enemy the cart picks up. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FScalableFloat SpiritDamage = 60.0f;

	/** Applied to dragged enemies when the cart hits a wall. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> StunEffect;

	/** Optional cosmetic montage (not waited on). */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> CastMontage;

	UFUNCTION()
	void OnProjectileSpawned(AProjectile* SpawnedProjectile);

	UFUNCTION()
	void OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile);
};
