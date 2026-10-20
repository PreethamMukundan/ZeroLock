// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "ZL_GATargetActor_CylinderCharge.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API AZL_GATargetActor_CylinderCharge : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()
public:
	AZL_GATargetActor_CylinderCharge();


	float CylinderRadius = 200.f;
	float CylinderHeight = 600.f;
	float RequiredTime = 1.5f;

	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual void ConfirmTargetingAndContinue() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:	UPROPERTY()
	TMap<AActor*, float> TrackedActors;


	UPROPERTY()
	TMap<AActor*, AGameplayAbilityWorldReticle*> SpawnedReticles;

	void UpdateTargetingData(float DeltaSeconds);
	void ManageReticles();
	bool IsValidTarget(AActor* Actor) const;


	FVector GetCylinderCenter() const;
	FVector GetAimDirection() const;

};
