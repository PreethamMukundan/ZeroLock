// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ZL_Task_WaitSlamLocation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitSlamLocationDelegate, const FVector&, ExactSlamLocation);

class AZeroLockCharacter;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_Task_WaitSlamLocation : public UAbilityTask
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintAssignable)
	FWaitSlamLocationDelegate OnLocationReceived;
	
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UZL_Task_WaitSlamLocation* FindSlamLocation(UGameplayAbility* owningAbility, FName TaskInstanceName,AZeroLockCharacter* owningCharacter,float baseRadius,float heightMultiplier,float maxRadius);
	
	virtual void Activate() override;
	void ReceiveLocationFromClient(const FVector& Vector);
	
protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;


	float TBaseRadius;
	float THeightMultiplier;
	float TMaxRadius;
	AZeroLockCharacter* OwningHero;
	void PerformTraceAndSendToServer();
	void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);
	void OnTargetDataCancelledCallback();
	bool GetLookAtLocation(const AZeroLockCharacter* InActor, float BaseRadius, float HeightMultiplier, float MaxAllowedRadius, FHitResult& OutHit);
};
