// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Curves/CurveVector.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Apollo_FlawlessAdvance.generated.h"

class UZL_AbilityTask_MoverMoveTo;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_Apollo_FlawlessAdvance : public UBaseGameplayAbility
{
	GENERATED_BODY()
	
public:
	UZL_Apollo_FlawlessAdvance();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;


	UFUNCTION()
	void OnInitialMoveComplete();
	void StartChargePhase();
	
	FVector GetExactInputDirection() const;

	UFUNCTION()
	void OnRelease(float TimeHeld);
	
	
   UFUNCTION()
   void OnChargeReleased(float TotalTime, bool bWasPerfect);

	void ExecuteLunge(bool bIsPerfect);

	UFUNCTION()
	void OnLungeFinished();
	
	class UZL_WaitDelay_Task* WaitTimeTask ;
	class UAbilityTask_PlayMontageAndWait* AnimMontageTask ;
	
	UPROPERTY()
	UZL_AbilityTask_MoverMoveTo* ActiveChargeMovementTask;

	UPROPERTY()
	UZL_AbilityTask_MoverMoveTo* LungeRootMotionTask;
	
	//class UAbilityTask_ApplyRootMotionMoveToForce* ActiveChargeMovementTask;
	//class UAbilityTask_ApplyRootMotionMoveToForce* LungeRootMotionTask;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Movement")
	UCurveVector* InitChargePathOffsetCurve;

	UFUNCTION()
	void OnRecastPressed(float TimeWaited);
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Stats")
	int32 MaxLunges = 3;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Stats")
	float ChargeTime =0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Stats")
	float RecastWindow = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Movement")
	float ChargeVelocity = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Damage")
	FScalableFloat BaseDamageValue =1;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Damage")
	FScalableFloat PerfectDamage =1;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Damage")
	FScalableFloat HealValue =1;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Timing")
	float MaxLungeDuration = 1.5f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Timing")
	float PerfectWindowMin = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Timing")
	float PerfectWindowMax = 0.9f;
	
	float ChargeStartTime;
	
	
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Movement")
	FScalableFloat LungeBurstVelocity= 2500.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lunge|Movement")
	TObjectPtr<UAnimMontage> ChargeMontage;

private:
	int32 CurrentLungeCount = 0;
	EMovementMode InitialMovementMode;
	
	void DetectLungeTargets(const FVector& StartLocation, const FVector& Direction, float Distance, bool bIsPerfect);
};
