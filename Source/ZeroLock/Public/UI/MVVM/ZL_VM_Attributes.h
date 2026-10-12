//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ZL_VM_Attributes.generated.h"

class UZL_VM_ChargePercent;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_VM_Attributes : public UMVVMViewModelBase
{
	GENERATED_BODY()


	//Health
public:
	float GetHealth() const {return Health;}
	void SetHealth(const float& health);

	float GetMaxHealth() const {return MaxHealth;}
	void SetMaxHealth(const float& maxhealth);

	UFUNCTION(BlueprintPure,FieldNotify)
	const float GetHealthPercentage()const { return Health/MaxHealth;}

private:
	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	float Health;

	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	float MaxHealth;



	//Ammo
public:
	int32 GetAmmo() const {return Ammo;}
	void SetAmmo(const int32& ammo);

	int32 GetMaxAmmo() const {return MaxAmmo;}
	void SetMaxAmmo(const int32& maxammo);


	bool GetIsInfiniteAmmo()const {return IsInfiniteAmmo;};
	void SetIsInfiniteAmmo(const bool& isInfinite);


	UFUNCTION(BlueprintPure,FieldNotify)
	const FString GetFinalAmmoText()const;
	
	UZL_VM_ChargePercent* GetVM_ChargePhase()const {return VM_ChargePhase;}
	void SetVM_ChargePhase(UZL_VM_ChargePercent* inVM);


private:
	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	int32 Ammo;

	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	int32 MaxAmmo;

	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	bool IsInfiniteAmmo;

	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	UZL_VM_ChargePercent* VM_ChargePhase;


	//Stamina
public:
	float GetStamina() const {return Stamina;}
	void SetStamina(const float& stamina);

	float GetMaxStamina() const {return MaxStamina;}
	void SetMaxStamina(const float& maxstamina);

	// Whole stamina points ready to spend (the pips that are full).
	UFUNCTION(BlueprintPure,FieldNotify)
	int32 GetStaminaCharges() const { return FMath::FloorToInt(Stamina + KINDA_SMALL_NUMBER); }

	// 0-1 fill of the stamina point that is currently regenerating (1 when stamina is full).
	UFUNCTION(BlueprintPure,FieldNotify)
	float GetStaminaRechargeProgress() const;

	UFUNCTION(BlueprintPure,FieldNotify)
	float GetStaminaPercentage() const { return MaxStamina > 0.0f ? Stamina / MaxStamina : 0.0f; }

	// "2/3"
	UFUNCTION(BlueprintPure,FieldNotify)
	FText GetStaminaText() const;

private:
	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	float Stamina;

	UPROPERTY(BlueprintReadOnly,FieldNotify,Getter,Setter,meta=(AllowPrivateAccess=true))
	float MaxStamina;

	void BroadcastStaminaDerived();

};
