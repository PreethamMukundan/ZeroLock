// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_Tempest_SideStep.generated.h"

class AZeroLockCharacter;

/**
 * Tempest's Side Step (sheet: Air Jordan). Leaps the way he's moving (WASD; forward if standing still): a LaunchDuration
 * kick of LaunchFlatSpeed + LaunchUpSpeed (owner, RequestSafeAbilityMove), then a natural arc under gravity.
 * Take-off: TakeoffDamage to enemies within HitRadius (server, LaunchCue). Landing: when this machine's Mover goes from an
 * air mode back to Walking/Sliding (double jumps and air dashes just extend the wait; LandingTimeout caps it),
 * LandingDamage + a LandingStun stun to enemies within HitRadius (server, LandCue). The client never replicates its end:
 * it lands before the server does.
 */
UCLASS()
class ZEROLOCK_API UZL_Tempest_SideStep : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_Tempest_SideStep();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	float LaunchFlatSpeed = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	float LaunchUpSpeed = 1000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	float LaunchDuration = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	FScalableFloat HitRadius = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	FScalableFloat TakeoffDamage = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	FScalableFloat LandingDamage = 70.0f;

	/** Stun length on landing. */
	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	FScalableFloat LandingStun = 0.8f;

	/** Seconds before the ability gives up waiting for a landing (no landing hit then). */
	UPROPERTY(EditDefaultsOnly, Category = "Side Step")
	float LandingTimeout = 10.0f;

	/** At take-off (owner predicted + server). Radius = HitRadius. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue LaunchCue;

	/** At the landing (owner plays its own copy at once; the server sends it to the rest). Radius = HitRadius. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue LandCue;

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDrawDebug = false;

private:
	UFUNCTION()
	void OnMovementModeChanged(const FName& PreviousMovementModeName, const FName& NewMovementModeName);

	UFUNCTION()
	void OnLandingTimeout();

	static bool IsGroundMode(const FName& ModeName);
	void Land();
	void UnbindLanding();
	void HitAround(AZeroLockCharacter* Hero, float Damage, float StunDuration) const;

	FVector LaunchDirection = FVector::ForwardVector;
	bool bLeftGround = false;
	bool bLandingBound = false;
};
