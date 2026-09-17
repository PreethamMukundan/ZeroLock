// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ZL_AbilityTask_Reach_MoverMoveTo.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoverReachMoveToDelegate,bool,Finished);
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_AbilityTask_Reach_MoverMoveTo : public UAbilityTask
{
	GENERATED_BODY()
	
	
public:
	UZL_AbilityTask_Reach_MoverMoveTo(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "ZeroLock|Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UZL_AbilityTask_Reach_MoverMoveTo* ApplyMoverReachMoveTo(UGameplayAbility* OwningAbility, FName TaskInstanceName, FVector StartLocation, FVector TargetLocation,float Duration,float DistDelta , float MaxDuration,bool bStopAtEnd);

	UPROPERTY(BlueprintAssignable)
	FMoverReachMoveToDelegate OnFinished;

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	FVector StartLoc;
	FVector TargetLoc;
	float MaxDuration;
	float MoveDuration;
	float FinishDistanceDelta;
	bool bStopInEnd;

	FTimerHandle FinishTimerHandle;

	UFUNCTION()
	void OnMoveComplete(bool ReachedLocation);
	
	UFUNCTION()
	void MaxTImerReached();
	
};
