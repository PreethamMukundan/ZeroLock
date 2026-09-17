// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mover/ZeroMoverPawn.h"
#include "Logging/LogMacros.h"
#include "AbilitySystemInterface.h"
#include <GameplayEffectTypes.h>
#include "GameplayAbilitySpec.h"
#include "ZeroLock.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZeroLockCharacter.generated.h"

class UZL_AbilityUIManagerComponent;
class UWidgetComponent;
class UZL_BaseDamageWidgetComponent;
class UZL_VM_Attributes;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHealthChangeDelgate,float ,currentHealth,float,MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStunChanged,bool,IsStunned);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamageRecievedDelegate,float,currentHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAddAbilityIconDelegate,UBaseGameplayAbility*,AbilitytoAdd,EGASAbilityInputID,slotToAddIn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeroGameplayEventDelegate, const FGameplayEventData&, EventData);

USTRUCT()
struct FMyAbilityMap
{
	GENERATED_BODY()

	UPROPERTY()
	UBaseGameplayAbility* Ability;

	UPROPERTY()
	EGASAbilityInputID InputID;
};

USTRUCT(BlueprintType)
struct FZL_DamageNumber
{
	GENERATED_USTRUCT_BODY()

	float DamageAmount;
	FGameplayTagContainer Tags;

	FZL_DamageNumber() {}
	FZL_DamageNumber(float InDamageAmount, FGameplayTagContainer InTags) : DamageAmount(InDamageAmount)
	{
		Tags.AppendTags(InTags);
	}
};

UCLASS(config=Game)
class AZeroLockCharacter : public AZeroMoverPawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Parry, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ParryComp;
	
	// Gameplay Ability system inputs
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_PrimaryFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_Reload;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_SecondryFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_Ability1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_Ability2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_Ultimate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_Parry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_Confirm;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_Cancel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_UIInfo;
	
public:
	AZeroLockCharacter();

protected:

	void MeleePressed();
	void MeleeReleased();
	void ParryPressed();
	void UIInfoPressed();
	void UIInfoReleased();

public:
	float MeleePressedTime;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee, meta = (AllowPrivateAccess = "true"))
	float MeleeMinHoldTime;
	FTimerHandle MeleePressedTimer;
	bool bMeleeUsed = false;

	void AddDamageNumber(float Damage, FGameplayTagContainer DamageNumberTags);
	TArray<FZL_DamageNumber> DamageNumberQueue;
	FTimerHandle DamageNumberTimer;
	virtual void ShowDamageNumber();

	UFUNCTION(BlueprintCallable)
	UZL_BaseDamageWidgetComponent* GetMyDamageNumberComp()const { return DamageWidgetComp; };

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UZL_BaseDamageWidgetComponent* DamageWidgetComp;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Stunned(FGameplayTag GameplayTag, int NewCount);
	void Parry(FGameplayTag GameplayTag, int NewCount);

	void HealthAttributeChanged(const FOnAttributeChangeData& OnAttributeChangeData);
	void AmmoAttributeChange(const FOnAttributeChangeData& OnAttributeChangeData);
	void SpeedAttributeChanged(const FOnAttributeChangeData& OnAttributeChangeData);
	void AddEventForDeath();
	
	virtual void BeginPlay() override;

public:
	FCollisionQueryParams GetIgnoreCharacterParams() const;
	void Death();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UBaseCharAbilitySystemComponent* AbilitySystemComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UZero_Item_Inventory_Component* ItemInventoryComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UZL_OverHeadWidgetComponent* OverHeadDisplay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UBaseCharAttributeSet* AttributeSet;

public:
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent()const override;
	virtual UBaseCharAbilitySystemComponent* GetMyAbilitySystemComp()const;
	virtual UBaseCharAttributeSet* GetMyAttributeSet()const;

	void MovementLocked(FGameplayTag GameplayTag, int NewCount);
	virtual void InitializeAttributes();
	UFUNCTION()
	void NewAbilityAddedLocal(FGameplayAbilitySpec& AbilitySpec);
	virtual void GiveAbilities();

	virtual void BroadcastAbilitiesToUI(UBaseGameplayAbility* Ability , EGASAbilityInputID InputID);
	void GrantAbilityOfClassX(TSubclassOf<class UBaseGameplayAbility> AbilityToGrant,EGASAbilityInputID InputToBindTo,bool brodcast = false);
	UZero_Item_Inventory_Component* GetInventoryComponent()const;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	bool bIsPrimaryPressed =false;
	void PrimaryFirePressed();
	void PrimaryFireReleased();
	float TimeOfLastShot;

	UFUNCTION(BlueprintCallable)
	bool IsAlive();

	void ChangeFireRate();
	void PrimaryFireTickFunction();
	FTimerHandle PrimaryFireTickHandle;

	void SecondryFirePressed();
	void SecondryFireReleased();

	void Ability_1Pressed();
	void Ability_1Released();

	void Ability_2Pressed();
	void Ability_2Released();

	void UltimateAbilityPressed();
	void UltimateAbilityReleased();

	void Reload();

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> DefaultGameplayEffect;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<class UBaseGameplayAbility>> DefaultAbilities;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UBaseGameplayAbility> PrimaryFireAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UBaseGameplayAbility> SecondryFireAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UBaseGameplayAbility> Ability_1;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UBaseGameplayAbility> Ability_2;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UBaseGameplayAbility> UltimateAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UBaseGameplayAbility> ReloadAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS/Melee")
	TSubclassOf<class UBaseGameplayAbility> LightMeleeAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS/Melee")
	TSubclassOf<class UBaseGameplayAbility> HeavyMeleeAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS/Melee")
	TSubclassOf<class UBaseGameplayAbility> ParryAbility;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "GAS")
	TArray<FGameplayAbilitySpecHandle> DefaultAbilitiesHandles;

	UPROPERTY(BlueprintAssignable)
	FHealthChangeDelgate HealthChangeDelegate;
	UPROPERTY(BlueprintAssignable)
	FHealthChangeDelgate AmmoChangeDelegate;
	UPROPERTY(BlueprintAssignable)
	FStunChanged StunChangedDelegate;
	UPROPERTY(BlueprintAssignable)
	FDamageRecievedDelegate DamageRecievedDelegate;
	UPROPERTY(BlueprintAssignable)
	FAddAbilityIconDelegate AddAbilityIconDelegate;
	
public:
	UFUNCTION()
	void HealthChanged(float currentH , float MaxH);
	UFUNCTION()
	void OnTakeDamage(float currentH);
	UFUNCTION()
	void HandleDeath();
	UFUNCTION(Server, Reliable)
	void ServerHandleDeath(APlayerController* PC);
	
	UPROPERTY(BlueprintReadOnly,Replicated,Category = "Assist")
	AZeroLockCharacter* LastHitCharacter;

	UPROPERTY(BlueprintReadOnly,Replicated,Category = "Assist")
	TArray<TObjectPtr<AZeroLockCharacter>> AssistListCharacters;

	UPROPERTY(BlueprintReadOnly,Category = "Assist")
	TMap<AZeroLockCharacter*, float> AssistTimeMap;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assist")
	float AssistWindow = 10.f;

	FTimerHandle AssistWindowTimerHandle;

	void AddLastHit(AZeroLockCharacter* Character);
	void ClearAssistList(float currentTime);

	UPROPERTY(Replicated)
	bool bIsDead = false;

	UFUNCTION()
	void OnDied(AController* Killer, AController* Victim);

	void ResetCharacter();
	void PassiveAbilityRestart(TSubclassOf<class UBaseGameplayAbility> AbilityToGrant);
	void ResetAllAbilities();

	UPROPERTY(Replicated)
	FVector StartLocation;

	UPROPERTY()
	TArray<FMyAbilityMap> AbilitiesArray;

	UFUNCTION()
	void OnRep_AbilityUIData();
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly, Category = "Animation/melee")
	UAnimMontage* LightMeleeMontage;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly, Category = "Animation/melee")
	UAnimMontage* HeavyMeleeMontage;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void FOVChange(float newFOV,float duration);

	TMap<EGASAbilityInputID,FGameplayTag> inputTags;

	UPROPERTY(EditDefaultsOnly, Category = "Icon")
	UTexture2D* Icon;

	void InitInputTagsMap();

	UPROPERTY(BlueprintAssignable, Category = "GAS|Events")
	FHeroGameplayEventDelegate OnWeaponHitEventReceived;
    
	UFUNCTION(BlueprintCallable)
	void HandleWeaponHitEvent(const FGameplayEventData& EventData);

	UPROPERTY(BlueprintReadWrite,Category="MVVM")
	UZL_VM_Attributes* VM_Attributes;

	UFUNCTION(BlueprintCallable)
	UZL_VM_Attributes* GetVM_Attributes();

	UFUNCTION(BlueprintCallable)
	UObject* GetVMObject_Attributes();

	UFUNCTION()
	void CreateVM_Att();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSubclassOf<class UZL_OverHeadDisplay> OverHeadClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UZL_OverHeadDisplay* OverHeadDisplayRef;
	
	UFUNCTION()
	void InitializeFloatingStatusBar();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UZL_AbilityUIManagerComponent* AbilityUIManager;
    
public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	UZL_AbilityUIManagerComponent* GetAbilityUIManager() const { return AbilityUIManager; }

	UFUNCTION(BlueprintCallable, Category = "Teams")
	bool IsOnSameTeam(AZeroLockCharacter* CharacterToCheck);
};