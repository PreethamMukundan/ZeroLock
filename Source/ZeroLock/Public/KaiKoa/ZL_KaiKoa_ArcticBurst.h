// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_KaiKoa_ArcticBurst.generated.h"

class AZeroLockCharacter;

/**
 * Kai Koa's Arctic Burst. Instant: an icy burst in a sphere (BurstRadius) centred BurstForwardOffset in front of him
 * along the flat aim. Every enemy in it with line of sight to the centre takes SpiritDamage and is slowed by SlowPercent
 * for SlowDuration (UZL_GE_ItemStatsTimed, MoveSlowPercent). Damage and slow on the server.
 */
UCLASS()
class ZEROLOCK_API UZL_KaiKoa_ArcticBurst : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_KaiKoa_ArcticBurst();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Burst")
	float BurstForwardOffset = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Burst")
	FScalableFloat BurstRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Burst")
	FScalableFloat SpiritDamage = 60.0f;

	/** 40 = 40% slower. */
	UPROPERTY(EditDefaultsOnly, Category = "Burst")
	FScalableFloat SlowPercent = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Burst")
	FScalableFloat SlowDuration = 1.5f;

	/** At the burst centre (owner predicted + server). Radius = BurstRadius. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue BurstCue;

	/** On each enemy hit (server; a loop for its Duration). */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FZL_KitCue ChillCue;

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDrawDebug = false;

private:
	void FindTargets(AZeroLockCharacter* Hero, const FVector& Center, float Radius, TArray<AZeroLockCharacter*>& OutVillans) const;
	void Chill(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan, const FVector& Center) const;
};
