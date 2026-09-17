// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Apollo_ItaniLoSahn.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_Apollo_ItaniLoSahn : public UBaseGameplayAbility
{
	GENERATED_BODY()
	
public:
	UZL_Apollo_ItaniLoSahn();

	UFUNCTION()
	void OnExecutecallback();
	UFUNCTION()
	void OnMoveToDone();
	UFUNCTION()
	void OnTimeFinish();
	UFUNCTION()
	void ReleaseInputRelease(float TimeHeld);
	UFUNCTION()
	void ChargedDone();
	
	UPROPERTY(EditDefaultsOnly,Category="ChargePhase")
	float ChargeupTime = 4.0f;
	
	UPROPERTY(EditDefaultsOnly,Category="ChargePhase")
	FScalableFloat MaxTargetRange = 1;
	
	UPROPERTY(EditDefaultsOnly,Category="Damage")
	float DamageRadius = 500;
	
	UPROPERTY(EditDefaultsOnly,Category="Damage")
	FScalableFloat Damage = 1.0f;
	
	UPROPERTY(EditDefaultsOnly,Category="Damage")
	FScalableFloat BonusDamagePercent = 1.0f;
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	class UZL_WaitDelay_Task* WaitTimeTask ;
	class UZL_AbilityTask_MoverMoveTo* MoveToTask;
	class UAbilityTask_WaitDelay* TempWaitTask;
	
	
	float HeightSave;
	float RadiusSave;
	
	TArray<TWeakObjectPtr< AZeroLockCharacter>> HitTargets;
	FGameplayAbilityTargetDataHandle CurrentTargetData;
	
	UPROPERTY()
	class USphereComponent* DashDetectionSphere;

	TSet<AActor*> UniqueHitActors; 
	
	UFUNCTION()
	void OnDashSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
};
