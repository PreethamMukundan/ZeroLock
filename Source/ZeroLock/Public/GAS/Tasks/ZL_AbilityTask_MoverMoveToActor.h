// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ZL_AbilityTask_MoverMoveToActor.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMoverMoveToActorDelegate);
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_AbilityTask_MoverMoveToActor : public UAbilityTask
{
	GENERATED_BODY()
	
public:
	UZL_AbilityTask_MoverMoveToActor(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(BlueprintAssignable)
	FMoverMoveToActorDelegate OnTargetReached;
	
	UPROPERTY(BlueprintAssignable)
	FMoverMoveToActorDelegate OnTimeExpired;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UZL_AbilityTask_MoverMoveToActor* ApplyMoverMoveToActor(UGameplayAbility* OwningAbility, FName TaskInstanceName, AActor* TargetActor, float Duration, float AcceptanceRadius = 100.0f);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

protected:
	UPROPERTY()
	AActor* TargetToTrack;

	float MoveDuration;
	float AcceptRadius;
	
};
