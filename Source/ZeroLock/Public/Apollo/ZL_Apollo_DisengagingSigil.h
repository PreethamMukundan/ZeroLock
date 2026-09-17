// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BasePlayAnimation_AndDo.h"
#include "ZL_Apollo_DisengagingSigil.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_Apollo_DisengagingSigil : public UZL_BasePlayAnimation_AndDo
{
	GENERATED_BODY()
	
public:
	UZL_Apollo_DisengagingSigil();

	UFUNCTION()
	void OnMoveComplete();
	virtual void OnAnimationPointTrigger() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float KnockbackStrength=1000;
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FScalableFloat BaseDamageValue =1;
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float EffectRadius =100.0f;
	
	
	UPROPERTY(EditDefaultsOnly, Category = "Montage")
	TObjectPtr<UAnimMontage> KnockBackMontage;
	
	
};
