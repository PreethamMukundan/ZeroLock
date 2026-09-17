// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ZL_AbilityTask_MoverMoveTo.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMoverMoveToDelegate);
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_AbilityTask_MoverMoveTo : public UAbilityTask
{
	GENERATED_BODY()
	
public:
	UZL_AbilityTask_MoverMoveTo(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "ZeroLock|Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UZL_AbilityTask_MoverMoveTo* ApplyMoverMoveTo(UGameplayAbility* OwningAbility, FName TaskInstanceName, FVector StartLocation, FVector TargetLocation, float Duration,bool bStopAtEnd);

	UPROPERTY(BlueprintAssignable)
	FMoverMoveToDelegate OnFinished;

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	FVector StartLoc;
	FVector TargetLoc;
	float MoveDuration;
	bool bStopInEnd;

	FTimerHandle FinishTimerHandle;

	UFUNCTION()
	void OnMoveComplete();
	
};
