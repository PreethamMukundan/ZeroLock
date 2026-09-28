// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/Base_GA_TargetActors.h"
#include "ZL_Lash_DeathSlam.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_Lash_DeathSlam : public UBase_GA_TargetActors
{
	GENERATED_BODY()
public:
	UZL_Lash_DeathSlam();

	UFUNCTION()
	void TargetTimeOut();
	/** Overriding Activate to inject our custom Cylinder logic */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected:
	/** Time in seconds an enemy must stay in the cylinder to be "Locked" */
	UPROPERTY(EditAnywhere, Category = "Targeting|Cylinder")
	float LockOnThreshold = 1.5f;


	UPROPERTY(EditAnywhere, Category = "Targeting|Cylinder")
	float CylinderRadius = 200.0f;


	UPROPERTY(EditAnywhere, Category = "Targeting|Cylinder")
	float CylinderLength = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<UGameplayEffect> LockOnEffect;
	TArray<TWeakObjectPtr<AActor>> OutActors;

	UPROPERTY(EditAnywhere, Category = "Effects")
	float PullTime =0.5;

	UPROPERTY(EditAnywhere, Category = "Effects")
	FScalableFloat SlamDamage;

	UPROPERTY(EditAnywhere, Category = "Effects")
	FScalableFloat SlamRadius;

	UFUNCTION()
	void TargetSlamFinish();
	UFUNCTION()
	void SlamLocationReceived(const FVector& ExactSlamLocation);

	UFUNCTION()
	void TargetPullFinish();


	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<UGameplayEffect> AfterSlamEffect;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> CurrentActiveEffectHandles;
	virtual void AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data) override;

	static bool GetLookAtLocation(const AZeroLockCharacter* InActor, float BaseRadius, float HeightMultiplier, float MaxAllowedRadius, FHitResult& OutHit);
	
	class UZL_WaitDelay_Task* InitWaitTask;
};
