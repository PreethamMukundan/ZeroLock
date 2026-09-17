//Copyright Preetham Mukundan (C) 2026

#include "GAS/ZeroBase_HeavyMelee.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/GAST_MeleeMoveTo.h"
#include "ZeroLock/ZeroLockCharacter.h"
UZeroBase_HeavyMelee::UZeroBase_HeavyMelee()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UZeroBase_HeavyMelee::OnFinish()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UZeroBase_HeavyMelee::MeleeDistanceFinished()
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (Hero)
    {
        MeleeDamageExec(HeavyMeleeDamageEffect, Hero->HeavyMeleeMontage, 1000);
    }
}

void UZeroBase_HeavyMelee::WaitOver()
{
    if (InputRTask && InputRTask->IsActive())
    {
       InputRTask->EndTask();
    }
    StartHeavyMelee();
}

void UZeroBase_HeavyMelee::OnInputRelased(float TimeHeld)
{
    if (TaskWaitDelay && TaskWaitDelay->IsActive())
    {
       TaskWaitDelay->EndTask();
    }
    
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());

   // if (Hero && Hero->GetCharacterMovement())
   // {
   //     Hero->GetCharacterMovement()->MaxWalkSpeed = 600.f; 
  //  }

    if (TimeHeld >= MeleeMinHoldTime)
    {
       StartHeavyMelee();
    }
    else
    {
       MeleeDamageExec(LightMeleeDamageEffect, Hero ? Hero->LightMeleeMontage : nullptr);
    }
}

void UZeroBase_HeavyMelee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
 
	StartHeavyMelee();

}

void UZeroBase_HeavyMelee::StartHeavyMelee()
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero) return;
	
    if (Hero->IsLocallyControlled())
    {
        Hero->FOVChange(130, 0.25);
    }

    MeleeMoveTask = UGAST_MeleeMoveTo::MeleeToLocation(this, FName("HeavyMelee"), MeleeTime, MeleeSpeed);
    MeleeMoveTask->OnMeleeMoveFinished.AddDynamic(this, &UZeroBase_HeavyMelee::MeleeDistanceFinished);
    MeleeMoveTask->ReadyForActivation();
}

void UZeroBase_HeavyMelee::MeleeDamageExec(TSubclassOf<class UGameplayEffect> MeleeDamageEffect, UAnimMontage* MeleeMontage, float KnockForce)
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero) return;

    if (MeleeMontage)
    {
       UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, FName("HeavyMelee"), MeleeMontage);
       MontageTask->OnCompleted.AddDynamic(this, &UZeroBase_HeavyMelee::OnFinish);
       MontageTask->OnCancelled.AddDynamic(this, &UZeroBase_HeavyMelee::OnFinish);
       MontageTask->ReadyForActivation();
    }

    if (Hero->IsLocallyControlled())
    {
        Hero->FOVChange(120, 0.25);
    }

    if (Hero->HasAuthority())
    {
        UAbilitySystemComponent* AbilitySystemComp = Hero->GetAbilitySystemComponent();
        
        FCollisionShape Cap = FCollisionShape::MakeSphere(150.f); 
        FVector OverlapLocation = Hero->GetActorLocation() + Hero->GetActorForwardVector() * 100.f;
        TArray<FOverlapResult> Overlaps;
        
        if(GetWorld()->OverlapMultiByObjectType(Overlaps, OverlapLocation, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1), Cap))
        {
           FGameplayTag ParryTag = FGameplayTag::RequestGameplayTag(FName("ZeroLock.Melee.Parry"), false);
           TArray<AZeroLockCharacter*> HitActors;

           for(const FOverlapResult& Overlap : Overlaps)
           {
              AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
              
              if (!Villan || Villan == Hero || HitActors.Contains(Villan)) continue;
              
              HitActors.Add(Villan);
              
              if (Villan->GetAbilitySystemComponent()->HasMatchingGameplayTag(ParryTag))
              {
                 if (AbilitySystemComp && ParryEffect)
                 {
                    ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ParryEffect.GetDefaultObject(), 1.0f);
                 }
                 break; 
              }
              
              if (AbilitySystemComp && MeleeDamageEffect)
              {
                 Hero->GetMyAbilitySystemComp()->ApplyMeleeDamage(Villan->GetMyAbilitySystemComp(), 90);
                 
                 FVector KnockbackDir = (Villan->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal();
                 KnockbackDir.Z = 0.2f; 
                 //Villan->LaunchCharacter(KnockbackDir * KnockForce, false, false);
              }
           }
        }
    }

    if (!MeleeMontage)
    {
       EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }
}