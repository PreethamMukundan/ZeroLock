//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BaseCharAbilitySystemComponent.generated.h"

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
public:
	UPROPERTY(BlueprintAssignable)
	FNewAbilityAddedDelegate OnNewAbilityAdded;

	UPROPERTY(BlueprintAssignable)
	FAbilityUpgradedDelegate OnAbilityUpgraded;

	UFUNCTION()
	void LevelUpAbility(UGameplayAbility* AbilityToUpgrade,int32 mLevel);
	
	UFUNCTION(BlueprintCallable)
	void ApplyWeaponDamage(UAbilitySystemComponent* TargetASC,float DamageValue);

	UFUNCTION(BlueprintCallable)
	void ApplySpiritDamage(UAbilitySystemComponent* TargetASC,float DamageValue);

	UFUNCTION(BlueprintCallable)
	void ApplyMeleeDamage(UAbilitySystemComponent* TargetASC,float DamageValue);

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
 

	void MarkActiveGameplayEffectDirty(FActiveGameplayEffect* ActiveGE);
 
	void CheckActiveEffectDuration(const FActiveGameplayEffectHandle& Handle);
};
