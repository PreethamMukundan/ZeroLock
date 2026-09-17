// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_Apollo_Riposte.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UZL_WaitDelay_Task;
class UGameplayEffect;

UCLASS()
class ZEROLOCK_API UZL_Apollo_Riposte : public UBaseGameplayAbility
{
    GENERATED_BODY()

public:
    UZL_Apollo_Riposte();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- Targeting Properties ---
    UPROPERTY(EditDefaultsOnly, Category = "Targeting")
    TSubclassOf<AGameplayAbilityTargetActor_Trace> TargetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Targeting")
    TSubclassOf<AGameplayAbilityWorldReticle> ReticleClassX;

    // --- Ability Properties ---
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TSubclassOf<UGameplayEffect> ResistanceEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TSubclassOf<UGameplayEffect> StunEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TSubclassOf<UGameplayEffect> ParryEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* ParryAnimMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Timing")
    float MaxTime = 2.0f;

    // --- Active Tasks ---
    UPROPERTY()
    UAbilityTask_PlayMontageAndWait* AnimMontageTask;

    UPROPERTY()
    UZL_WaitDelay_Task* WaitTimeTask;
    
    UPROPERTY()
    class UAbilityTask_WaitTargetData* WaitTargetTask;
    
    UPROPERTY()
    class UAbilityTask_WaitInputPress* InputPressedTask;
    
    TArray<TWeakObjectPtr<AActor>> OutActors;

    // --- Ability Callbacks ---
    UFUNCTION()
    void OnPlayerHit(AActor* HitActor);

    

    UFUNCTION()
    void OnInputPressed(float TimeWaited);
    UFUNCTION()
    void OnEventRecived(FGameplayEventData Payload);

    UFUNCTION()
    void OnTargetReached();
    // --- Targeting Callbacks ---
    UFUNCTION()
    void OnTargetConfirmed(const FGameplayAbilityTargetDataHandle& Data);

    UFUNCTION()
    void OnTargetCancelled(const FGameplayAbilityTargetDataHandle& Data);

  
};