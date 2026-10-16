//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_AbilityUIManagerComponent.generated.h"



class UBaseGameplayAbility;
class UAbilitySystemComponent;
struct FGameplayEffectSpec;
struct FActiveGameplayEffectHandle;
struct FGameplayAbilitySpec;
class UZL_VM_AbilitiesContainer;
class UZL_VM_AbilityIcon;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZEROLOCK_API UZL_AbilityUIManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UZL_AbilityUIManagerComponent();

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	UObject* GetAbilitiesViewModel() ;

	UFUNCTION()
	void AbilityUpgradeCallBackFromUI(UZL_VM_AbilityIcon* AbilityIconVM, int32 NewLevel);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_UpgradeAbility(TSubclassOf<UBaseGameplayAbility> AbilityClass, int32 NewLevel);

	UFUNCTION()
	void AbilityUpgradeCallBackFromASC(UGameplayAbility* AbilityLeveledUp, int32 NewLevel);
	
	UFUNCTION()
	void OnAbilityAdded(FGameplayAbilitySpec& Spec);

	/** Binds the owner's ASC events and replays its abilities, once, if the owner is locally controlled.
	 *  BeginPlay tries it; a pawn possessed after BeginPlay (hero swap) needs AZeroLockCharacter::NotifyHUDHeroReady to call it again. */
	void BindToLocalHero();
protected:
	virtual void BeginPlay() override;

	bool bBoundToHeroASC = false;

	UPROPERTY()
	TMap<FGameplayTag, UZL_VM_AbilityIcon*> StackTagToSlotMap;

	UPROPERTY()
	TMap< UZL_VM_AbilityIcon*,UGameplayAbility*> SlotToAbilityMap;

	// Callback for when stack tags change
	void OnStackTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UPROPERTY()
	TMap<FGameplayTag, UZL_VM_AbilityIcon*> AbilityTagMap;

	void InitializeTagMap();


	void OnStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount);


	void OnGEApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);


	UPROPERTY()
	TMap<FActiveGameplayEffectHandle, UZL_VM_AbilityIcon*> ActiveHandleToSlotMap;

	void OnCooldownTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void RefreshCooldowns();

	UPROPERTY()
	UZL_VM_AbilitiesContainer* VM_Abilities;
private:

	TMap<FGameplayTag, UZL_VM_AbilityIcon*> TagToSlotMap;
	FTimerHandle CooldownTimerHandle;

public:
	UZL_VM_ProgressionStack* GetAbilityProgressionStack();
	
	UZL_VM_AbilityTimerProgressBar* AddProgressBarVM(FName Abilityname);
	void RemoveProgressBarVM(UZL_VM_AbilityTimerProgressBar* AbilitynameToRemove);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MVVM")
	UZL_VM_ProgressionStack* VM_ProgressBarStack;


protected:

	void OnChargeAttributeChanged(const FOnAttributeChangeData& Data, UZL_VM_AbilityIcon* SlotVM);
	void OnMaxChargeAttributeChanged(const FOnAttributeChangeData& Data, UZL_VM_AbilityIcon* SlotVM);

	FGameplayAttribute GetChargeAttributeForSlot(EGameplayAbilitySlot Slot) const;
	FGameplayAttribute GetMaxChargeAttributeForSlot(EGameplayAbilitySlot Slot) const;
	
	UFUNCTION()
	void OnAbilityIconChanged(EGameplayAbilitySlot Slot, UTexture2D* newIcon);
		
};
