//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ZL_GameplayTags.h"
#include "Abilities/GameplayAbility.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ZeroLock/ZeroLock.h"
#include "BaseGameplayAbility.generated.h"

class UZL_VM_ChargePercent;
class UZL_VM_Attributes;
class UZL_VM_AbilityTimerProgressBar;
class UZL_VM_ProgressionStack;
class AZeroLockCharacter;
class UImage;
class APredictedProjectile;
class UBaseCharAbilitySystemComponent;

UENUM(BlueprintType)
enum class EGameplayAbilitySlot : uint8
{
	AbilitySlot1 UMETA(DisplayName = "Ability Slot 1"),
	AbilitySlot2 UMETA(DisplayName = "Ability Slot 2"),
	AbilitySlot3 UMETA(DisplayName = "Ability Slot 3"),
	UltimateSlot UMETA(DisplayName = "Ultimate Slot "),
};


UCLASS()
class ZEROLOCK_API UBaseGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBaseGameplayAbility();

	void SetSlot(EGameplayAbilitySlot slot);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	EGASAbilityInputID AbilityInputID = EGASAbilityInputID::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	EGASTargetConfirmationStyle TargetStyle = EGASTargetConfirmationStyle::Instant;

public:
	UFUNCTION()
	void ApplyGameplayEffectToTarget(TSubclassOf<UGameplayEffect> GEToApply,UAbilitySystemComponent* TargetASC,UAbilitySystemComponent* SourceASC);

	UFUNCTION()
	void ApplyGameplyEffectToSelf(TSubclassOf<UGameplayEffect> GEToApply,UAbilitySystemComponent* SourseASC);

	UPROPERTY()
	APredictedProjectile* PredictedProjectile;
	
	UFUNCTION()
	UAbilitySystemComponent* GetOwnerASC();

	//virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;


	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Icon")
	TObjectPtr<UTexture2D> IconImage;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Icon")
	FString AbilityName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Icon")
	FString AbilityDescription;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Icon")
	FString AbilityDescription1;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Icon")
	FString AbilityDescription2;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Icon")
	FString AbilityDescription3;
	

	TArray<FGameplayTag> AbilitySlotTags{ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_1,ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_2,ZerolockGameplayTagsForBinding::TAG_INPUT_SECONDRY,ZerolockGameplayTagsForBinding::TAG_INPUT_ULTIMATE};


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock|Ability")
	EGameplayAbilitySlot Slot;

	UFUNCTION()
	void SetInputID(EGASAbilityInputID in);

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual float GetCoolDownTime() const;
	
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Cooldown")
	FGameplayTagContainer CooldownTags;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Cooldown")
	FScalableFloat CooldownDuration;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Stacks")
	FGameplayTag StackTag;
	
	UPROPERTY(Transient)
	FGameplayTagContainer TempCooldownTags;

	
	static bool ReverseConeTraceMulti(const UObject* WorldContextObject, const FVector Start,const FRotator Direction, float ConeHeight, float ConeHalfAngle, ETraceTypeQuery TraceChannel, bool bTraceComplex,const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, TArray<AZeroLockCharacter*>& OutVillans,bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime= 5.0f );
	static bool ConeTraceMulti(const UObject* WorldContextObject, const FVector Start, const FRotator Direction, float ConeHeight, float ConeHalfAngle, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, TArray<AZeroLockCharacter*>& OutVillans, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f );

	bool GetConeOverlap(UWorld* World, TArray<FOverlapResult>& OutResults, const FVector& Origin, const FVector& Direction, float Radius, float AngleDegrees, ECollisionChannel Channel);

public:

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock|Charges")
	bool bIsChargedAbility = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock|Charges", meta = (EditCondition = "bIsChargedAbility"))
	float MaxChargesConfig = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock|Charges", meta = (EditCondition = "bIsChargedAbility"))
	float ChargeRechargeDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock|Charges", meta = (EditCondition = "bIsChargedAbility"))
	TSubclassOf<UGameplayEffect> ChargeRechargeGEClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock|Charges", meta = (EditCondition = "bIsChargedAbility"))
	FGameplayTag RechargeDurationTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock")
	class UZL_AbilityUIManagerComponent* MyAbilityManagerComp;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock")
	UZL_VM_AbilityTimerProgressBar* MyProgressBarVM;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroLock")
	UZL_VM_ChargePercent* MyChargePhaseVM;
	
	UFUNCTION()
	void StartProgressionTimer();
	
	UFUNCTION()
	UZL_AbilityUIManagerComponent* GetAbilityUiComp();
	
	UFUNCTION()
	void StopProgressionTimer();
	
	UFUNCTION()
	void UpdateProgressionTimer(float progress);
	
	UFUNCTION()
	void StartChargePhaseUI(float MaxTIme,float perfectmin,float perfectmax);
	UFUNCTION()
	void UpdateChargePhaseUI( float Progress, bool bIsPerfect, float ElapsedTime);
	UFUNCTION()
	void RemoveChargePhaseUI(bool wasPerfect);
	
	UFUNCTION()
	UZL_VM_Attributes* GetHeroAttributeVM();
	
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	
	static void SendVictimMoveEvent(AZeroLockCharacter* Hero, AZeroLockCharacter* Victim,const TCHAR* TagName, float Magnitude, const FVector* Location = nullptr);
protected:
	
	FGameplayAttribute GetChargeAttribute() const;
	FGameplayAttribute GetMaxChargeAttribute() const;
};
