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
#include "Weapon/ZL_WeaponStats.h"
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
DECLARE_MULTICAST_DELEGATE_OneParam(FOnZeroPreDeath, class AZeroLockCharacter* /*DeadHero*/);

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

	/** Light melee (LightMeleeAbility, InputID LightMelee). Defaults to IA_LightMelee. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* EI_LightMelee;
	
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

	/** From UZeroMeleeMode::OnMeleeLungeEnded (fired inside the Mover sim): on the server, runs MeleeStrike next tick. */
	void HandleMeleeLungeEnded(AActor* LungeHitActor, const FVector& LungeStart);
	/** Server: heavy melee, sweeps the lunge path (LungeStart to MeleeStrikeRange past the hero). */
	void MeleeStrike(TWeakObjectPtr<AActor> LungeHitActor, FVector LungeStart);

	void LightMeleePressed();
	void LightMeleeReleased();
	/** GetHeavyMeleeTriggerCount() when the light melee key went down. */
	int32 LightMeleePressHeavyCount = 0;

public:
	/** Server: sphere sweep Start→End (Hero + Pawn object types) plus ExtraTarget. Cubed heroes (Viscous's Cube, any
	 * team; the cube actor counts as its hero) are pushed along the strike at CubePushSpeed instead of hit. Otherwise
	 * enemies only (no self, dead or teammates). If any of them is parrying (TAG_MELEE_PARRY) the hero gets
	 * ParriedStunEffect and nobody is hit; otherwise each takes Damage as melee damage. Used by heavy melee
	 * (MeleeStrike) and UZeroBase_LightMelee. */
	/** TelemetrySource: name of the strike in telemetry damage/kill stats. */
	void PerformMeleeStrike(const FVector& Start, const FVector& End, float Radius, float Damage, AActor* ExtraTarget = nullptr, const FString& TelemetrySource = TEXT("Melee"));

protected:

	void HealthAttributeChanged(const FOnAttributeChangeData& OnAttributeChangeData);
	void AmmoAttributeChange(const FOnAttributeChangeData& OnAttributeChangeData);
	void SpeedAttributeChanged(const FOnAttributeChangeData& OnAttributeChangeData);
	void StaminaAttributeChanged(const FOnAttributeChangeData& OnAttributeChangeData);
	/** Stamina from the mover simulation: mirrored into the attribute on the server, shown directly for the owning player. */
	void MoverStaminaChanged(float NewStamina, float MaxStamina);
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
	virtual void OnRep_Controller() override;

	/** Tells the local HUD this pawn is usable (locally controlled + PlayerState + ASC), once. Lets the HUD rebind after a hero swap. */
	void NotifyHUDHeroReady();
	bool bNotifiedHUDHeroReady = false;

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

	/** Primary weapon shape: pellets, burst, damage falloff (UZL_GA_WeaponFire, AProjectile with bUseShooterWeaponStats). */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS|Weapon")
	FZL_HeroWeaponStats WeaponStats;

	/** Attribute value if the hero has the attribute set and it is above 0, else Fallback (the ability/C++ default). */
	float GetAttributeOr(const FGameplayAttribute& Attribute, float Fallback) const;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS/Melee")
	TSubclassOf<class UBaseGameplayAbility> LightMeleeAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS/Melee")
	TSubclassOf<class UBaseGameplayAbility> HeavyMeleeAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GAS/Melee")
	TSubclassOf<class UBaseGameplayAbility> ParryAbility;

	/** Melee strike at the end of the Mover lunge (server): damage dealt to each enemy in front. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS/Melee|Strike")
	float MeleeStrikeDamage = 90.f;

	/** How far in front of the hero the strike reaches (from the capsule edge). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS/Melee|Strike")
	float MeleeStrikeRange = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS/Melee|Strike")
	float MeleeStrikeRadius = 60.f;

	/** Applied to the attacker when the strike lands on an enemy that is parrying (ZeroLock.Melee.Parry). Should grant
	 * ZeroLock.Stun. Defaults to BPGE_Stun. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS/Melee|Strike")
	TSubclassOf<UGameplayEffect> ParriedStunEffect;

	/** A melee strike on a cubed hero (Viscous's Cube) pushes the cube: impulse along the strike (cm/s), plus CubePushLift up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS/Melee|Strike")
	float CubePushSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS/Melee|Strike")
	float CubePushLift = 200.f;

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

	/** Server: a cubed player pressed jump (Viscous's Cube listens and breaks open). */
	FSimpleMulticastDelegate OnServerJumpEscapeRequested;

	UFUNCTION(Server, Reliable)
	void Server_RequestJumpEscape();

	/** Local jump press: while cubed, swallow it and ask the server to escape. */
	void HandleJumpForEscape();

	/** Server: fired by AZero_BaseGameModeBase::HandlePlayerDeath before kill credit, Death() and the respawn timer
	 * (e.g. Doorman's hotel takes its guest out first). */
	FOnZeroPreDeath OnServerPreDeath;

	/**
	 * Server: teleports this hero to Target through its own movement input (predicted, any distance: the target is
	 * authorized on the server and the owning client) and points the controller at ControlRotation.
	 */
	void ServerForceTeleport(const FVector& Target, const FRotator& ControlRotation);

	UFUNCTION(Client, Reliable)
	void ClientForceTeleport(FVector Target, FRotator ControlRotation);

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