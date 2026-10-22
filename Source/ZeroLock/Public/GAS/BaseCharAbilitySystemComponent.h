//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BaseCharAbilitySystemComponent.generated.h"

class UZL_Task_WaitSlamLocation;
class UBaseGameplayAbility;
/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNewAbilityAddedDelegate,FGameplayAbilitySpec& ,AbilitySpec);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityUpgradedDelegate,UGameplayAbility* ,Ability,int32 ,newlevel);
UCLASS()
class ZEROLOCK_API UBaseCharAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

	UFUNCTION()
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	


	virtual void OnRep_ActivateAbilities() override;

	/** Server: counts ability uses for telemetry. */
	virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	
protected:
	TMap<FPredictionKey, TWeakObjectPtr<UZL_Task_WaitSlamLocation>> PendingSlamTasks;
	
public:
	UPROPERTY(BlueprintAssignable)
	FNewAbilityAddedDelegate OnNewAbilityAdded;

	UPROPERTY(BlueprintAssignable)
	FAbilityUpgradedDelegate OnAbilityUpgraded;

	UFUNCTION()
	void LevelUpAbility(UGameplayAbility* AbilityToUpgrade,int32 mLevel);
	
	/** DamageSource (optional): the ability, projectile or actor dealing the damage; telemetry groups damage by its class. */
	UFUNCTION(BlueprintCallable)
	void ApplyWeaponDamage(UAbilitySystemComponent* TargetASC,float DamageValue, const UObject* DamageSource = nullptr);

	UFUNCTION(BlueprintCallable)
	void ApplySpiritDamage(UAbilitySystemComponent* TargetASC,float DamageValue, const UObject* DamageSource = nullptr);

	UFUNCTION(BlueprintCallable)
	void ApplyMeleeDamage(UAbilitySystemComponent* TargetASC,float DamageValue, const UObject* DamageSource = nullptr);

	/** Telemetry name of the damage being applied right now: set by Apply*Damage around the apply, read in PostGameplayEffectExecute. */
	FString TelemetryDamageSource;

	UFUNCTION(BlueprintCallable)
	void ApplyHeal(UAbilitySystemComponent* TargetASC,float HealValue);

	UFUNCTION(BlueprintCallable)
	void ApplyGameplayEffect(UAbilitySystemComponent* TargetASC,TSubclassOf<UGameplayEffect> EffectClass,int32 level);

	UFUNCTION(BlueprintCallable)
	void ApplyGameplayEffectWithStacks(UAbilitySystemComponent* TargetASC,TSubclassOf<UGameplayEffect> EffectClass,int32 level,int32 StackNum);

	UFUNCTION(Client, Reliable)
	void Client_SendGameplayEventToOwner(FGameplayTag Tag, FGameplayEventData Payload);
	
	UFUNCTION(BlueprintCallable)
	bool IsUntouchable(UAbilitySystemComponent* TargetToCheck);

	UFUNCTION(BlueprintCallable)
	void SendGameplayEventToSelf(FGameplayTag Tag, UAbilitySystemComponent* TargetASC);

	UFUNCTION(BlueprintCallable)
	void SendGameplayEventToTarget(FGameplayTag Tag , UAbilitySystemComponent* TargetASC);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,Category="GE/WeaponDamage")
	TSubclassOf<UGameplayEffect> GE_WeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,Category="GE/SpiritDamage")
	TSubclassOf<UGameplayEffect> GE_SpiritClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,Category="GE/MeleeDamage")
	TSubclassOf<UGameplayEffect> GE_MeleeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,Category="GE/Healing")
	TSubclassOf<UGameplayEffect> GE_HealingClass;



	UFUNCTION(BlueprintCallable)
	void AdjustActiveEffectsDurationByPercentage(float Multiplier);

	UFUNCTION(BlueprintCallable)
	void AdjustActiveEffectsDurationByValue(float reductionAmount);

	
	FActiveGameplayEffect* GetActiveGameplayEffect_Mutable(FActiveGameplayEffectHandle Handle);
 

	TArray<FActiveGameplayEffectHandle> GetAllActiveEffectHandles() const;
 

	

	UFUNCTION(Server, Reliable)
	void ServerReportSlamLocation(FPredictionKey ActivationKey, FVector Location);
	
	TMap<FPredictionKey, FVector> CachedSlamLocations;
	
	void RegisterPendingSlamTask(const FPredictionKey& Key, UZL_Task_WaitSlamLocation* Task);
	void UnregisterPendingSlamTask(const FPredictionKey& Key);
	
	void MarkActiveGameplayEffectDirty(FActiveGameplayEffect* ActiveGE);
 
	void CheckActiveEffectDuration(const FActiveGameplayEffectHandle& Handle);
	
	/** Consumes cached TargetData from client (only TargetData) and returns whether any data was actually consumed. */
	bool TryConsumeClientReplicatedTargetData(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Server: spends one charge of a charged ability and starts its recharge if none is running. Charges come back one
	 * at a time: each recharge is a UZL_GE_BaseCooldown granting RechargeTag (so the owning client's UI can read its
	 * time), and when it expires it adds a charge and starts the next one until the ability is full. */
	void ConsumeAbilityCharge(UBaseGameplayAbility* Ability, const FGameplayAttribute& ChargeAttr, const FGameplayAttribute& MaxChargeAttr, FGameplayTag RechargeTag);

protected:
	struct FZLChargeRecharge
	{
		TWeakObjectPtr<UBaseGameplayAbility> Ability;
		FGameplayAttribute ChargeAttr;
		FGameplayAttribute MaxChargeAttr;
		FActiveGameplayEffectHandle Handle;
	};
	/** Server only, keyed by recharge tag (one per ability slot). */
	TMap<FGameplayTag, FZLChargeRecharge> ChargeRecharges;

	void StartChargeRecharge(FGameplayTag RechargeTag);
	void OnChargeRechargeRemoved(const FGameplayEffectRemovalInfo& RemovalInfo, FGameplayTag RechargeTag);
};
