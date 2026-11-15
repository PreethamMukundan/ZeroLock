// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_BaseDashAbility.generated.h"

class AZeroLockCharacter;

UENUM(BlueprintType)
enum class EZL_DashDamageType : uint8
{
	Spirit,
	Weapon,
	Melee
};

/**
 * Dash along the camera look direction (pitch included), through enemies, hitting each one once.
 * The move is predicted on the owner's machine. Hits are found on the server, by sweeping the hero's capsule along the path
 * it really moved. Override the hooks in C++ or a BP subclass:
 *   GetDashDirection  where to go (default: the camera look direction)
 *   OnDashStart       start of the dash, on the owner's machine and the server
 *   OnDashHit         each enemy hit, server only (default: DashDamage of DashDamageType)
 *   OnDashEnd         end of the dash, on the owner's machine and the server. DashHits is complete on the server only
 */
UCLASS(Abstract)
class ZEROLOCK_API UZL_BaseDashAbility : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_BaseDashAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** How far the dash goes (cm), scaled by the hero's ability range. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	FScalableFloat DashDistance = 1000.0f;

	/** Seconds the dash takes. Speed = DashDistance / DashDuration. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	FScalableFloat DashDuration = 0.25f;

	/** Stop dead at the end. Off: the hero keeps the dash's speed and falls from there. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	bool bStopAtEnd = true;

	/** Damage to each enemy hit, used by the default OnDashHit. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit")
	FScalableFloat DashDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit")
	EZL_DashDamageType DashDamageType = EZL_DashDamageType::Spirit;

	/** Added to the hero's capsule radius and half height for the hit sweep (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit", meta = (ClampMin = 0))
	FScalableFloat HitPadding = 50.0f;

	/** Seconds between hit sweeps. Each sweep covers the whole path since the last one, so nothing is skipped. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit", meta = (ClampMin = 0.01))
	float HitCheckInterval = 0.02f;

	/** Extra seconds a remote client's dash is swept on the server: its move starts there later than the ability does. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit", meta = (ClampMin = 0))
	float ServerHitGraceTime = 0.1f;

	/** Draws every hit sweep and a sphere on each enemy hit, on the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit")
	bool bDrawDashDebug = false;

	/** Enemies hit so far this dash (server only). */
	UPROPERTY(BlueprintReadOnly, Category = "Dash")
	TArray<TObjectPtr<AZeroLockCharacter>> DashHits;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dash")
	FVector GetDashDirection() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Dash")
	void OnDashStart(const FVector& Direction);

	UFUNCTION(BlueprintNativeEvent, Category = "Dash")
	void OnDashHit(AZeroLockCharacter* Villan, const FHitResult& Hit);

	UFUNCTION(BlueprintNativeEvent, Category = "Dash")
	void OnDashEnd(bool bWasCancelled);

private:
	void CheckDashHits();
	void FinishDash();

	bool bDashActive = false;
	FVector LastSweepLocation = FVector::ZeroVector;
	FTimerHandle HitCheckTimer;
	FTimerHandle FinishTimer;
};
