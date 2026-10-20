//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ZL_VM_AbilityIcon.generated.h"

class UBaseGameplayAbility;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityLevelChanged, UZL_VM_AbilityIcon*,AbilityIconVM,int32,newLevel);

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_VM_AbilityIcon : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	UTexture2D* IconTexture;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "UI")
	float CooldownPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "UI")
	FText CooldownText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "UI")
	bool bIsOnCooldown = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	bool bHasStacks =false;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	int32 StackNum =0;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	int32 AbilityLevel = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	FText AbilityDescription;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	FText AbilityName;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	FText AbilityLevel1Description;
	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	FText AbilityLevel2Description;
	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	FText AbilityLevel3Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	float MaxCoolDownTime;


	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	int32 AbilityCharges;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	int32 MaxAbilityCharges;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "UI")
	bool bHasCharges;

	/** Charged abilities: progress of the charge being recharged (0 → 1), 0 when full. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "UI")
	float RechargePercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "UI")
	bool bIsRecharging = false;

	/** Next-charge timer (from UZL_AbilityUIManagerComponent::RefreshCooldowns). With no charges left, the cooldown fields show it. */
	void UpdateRecharge(float Remaining, float Duration);

	void SetAbilityCharges(int32 Charges);
	void SetMaxAbilityCharges(int32 Charges);
	void SetbHasCharges(bool mbHasCharges);

	int32 GetAbilityCharges()const {return AbilityCharges;};
	int32 GetMaxAbilityCharges()const {return MaxAbilityCharges;};
	bool GetbHasCharges()const {return bHasCharges;};


	void SetAbilityDescription(FText mAbilityDescription);
	void SetAbilityLevel1Description(FText mAbilityLevel1Description);
	void SetAbilityLevel2Description(FText mAbilityLevel2Description);
	void SetAbilityLevel3Description(FText mAbilityLevel3Description);
	void SetMaxCoolDownTime(float mMaxCoolDownTime);
	void SetAbilityName(FText mAbilityName);

	FText GetAbilityName() const{return AbilityName;}
	FText GetAbilityDescription()const { return AbilityDescription; }
	FText GetAbilityLevel1Description()const { return AbilityLevel1Description; }
	FText GetAbilityLevel2Description()const { return AbilityLevel2Description; }
	FText GetAbilityLevel3Description()const { return AbilityLevel3Description; }
	float GetMaxCoolDownTime()const { return MaxCoolDownTime; }
	
	void SetIconTexture(UTexture2D* NewIcon) ;
	UTexture2D* GetIconTexture()const  { return IconTexture; }
	void UpdateCooldown(float Remaining, float Duration);

	void SetbHasStacks(bool bstacks);
	bool GetbHasStacks()const { return bHasStacks; }

	

	void SetStackNum(int32 NewStackNum);
	int32 GetStackNum()const { return StackNum; }

	void IncrementAbilityLevel();
	void SetAbilityLevel(int32 NewAbilityLevel);
	int32 GetAbilityLevel()const { return AbilityLevel; }
	
	
	UFUNCTION(BlueprintPure,FieldNotify)
	float GetChargePercent()const { return MaxAbilityCharges > 0 ? float(AbilityCharges) / float(MaxAbilityCharges) : 0.f; }

	UPROPERTY(BlueprintAssignable)
	FAbilityLevelChanged OnAbilityLevelChanged;

private:
	/** Last cooldown and recharge times; RefreshCooldownDisplay picks which one the cooldown fields show. */
	float CooldownRemaining = 0.f;
	float CooldownDuration = 0.f;
	float RechargeRemaining = 0.f;
	float RechargeDuration = 0.f;

	void RefreshCooldownDisplay();
};
