// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Lash_GroundStrike.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_Lash_GroundStrike : public UBaseGameplayAbility
{
	GENERATED_BODY()
public:
	UZL_Lash_GroundStrike();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;


	static bool GetLookAtLocation(const AZeroLockCharacter* InActor, float BaseRadius, float HeightMultiplier, float MaxAllowedRadius, FHitResult& OutHit);

protected:
	/** Settings */
	UPROPERTY(EditAnywhere, Category = "Lash | SlamDam")
	float StrikeConeHeight = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Lash | SlamDam")
	float StrikeConeAngle = 50.f;

	UPROPERTY(EditAnywhere, Category = "Lash | SlamDam")
	FScalableFloat StrikeDistanceMultipler;
	
	UPROPERTY(EditAnywhere, Category = "Lash | SlamDam")
	FScalableFloat StrikeBaseDam;
	
	UPROPERTY(EditAnywhere, Category = "Lash | Settings")
	float MovementSpeed = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Lash | Effects")
	TSubclassOf<UGameplayEffect> StunEffectClass;

	UPROPERTY(EditAnywhere, Category = "Lash | Visuals")
	TObjectPtr<UAnimMontage> SlamMontage;
	
	UPROPERTY()
	TObjectPtr<class UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	/** Internal State */
	UPROPERTY()
	TObjectPtr<class USphereComponent> ActiveDragSphere;

	UPROPERTY()
	TArray<AActor*> CaughtVictims;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> VictimStunHandles;

	FVector CurrentDashDestination;

	float DistanceTravelled;

protected:
	UPROPERTY(EditAnywhere, Category = "Lash | Scaling")
	float BaseRadius = 600.f;

	/** How much the radius grows per unit of height. e.g. 0.5 means 100 units of height adds 50 units of radius. */
	UPROPERTY(EditAnywhere, Category = "Lash | Scaling")
	float HeightScaleMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Lash | Scaling")
	float MaxAllowedRadius = 3000.f;
	UFUNCTION()
	void OnVictimCaught(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnMovementFinished(bool didreach);

	UPROPERTY(EditAnywhere, Category = "Lash | Scaling")
	TSubclassOf<UGameplayEffect> KnockUpEffect;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
};
