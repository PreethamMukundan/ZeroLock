//Copyright Preetham Mukundan (C) 2026

#include "GAS/ZeroBase_LightMelee.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/ZL_DeadlockAnimSet.h"
#include "Components/CapsuleComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/Tasks/GAST_PlayMontageAndWaitForEvent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZeroBase_LightMelee::UZeroBase_LightMelee()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Make the ability locally predicted so the client doesn't wait for the server to swing
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

UAnimMontage* UZeroBase_LightMelee::FindMontage() const
{
    if (LightMeleeMontage)
    {
        return LightMeleeMontage;
    }
    const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero)
    {
        return nullptr;
    }
    if (Hero->LightMeleeMontage)
    {
        return Hero->LightMeleeMontage;
    }
    return Hero->DeadlockAnimSet ? Hero->DeadlockAnimSet->LightMeleeMontage.Get() : nullptr;
}

void UZeroBase_LightMelee::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                           const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                           const FGameplayEventData* TriggerEventData)
{
    bStruck = false;
    if (!Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
       EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
       return;
    }

    UAnimMontage* Montage = FindMontage();
    if (!Montage)
    {
       // No animation set up yet: still hit, so the attack works.
       Strike();
       FinishAbility(false);
       return;
    }

    UGAST_PlayMontageAndWaitForEvent* MontageTask = UGAST_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
       this, FName("LightMelee"), Montage, FGameplayTagContainer(ZerolockGameplayTagsForBinding::TAG_EVENT_MELEE_HIT), MontagePlayRate);
    MontageTask->EventReceived.AddDynamic(this, &UZeroBase_LightMelee::OnHitEvent);
    MontageTask->OnBlendOut.AddDynamic(this, &UZeroBase_LightMelee::OnMontageEnded);
    MontageTask->OnCompleted.AddDynamic(this, &UZeroBase_LightMelee::OnMontageEnded);
    MontageTask->OnInterrupted.AddDynamic(this, &UZeroBase_LightMelee::OnMontageCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &UZeroBase_LightMelee::OnMontageCancelled);
    MontageTask->ReadyForActivation();

    // Fallback timing for montages without an Event.Melee.Hit notify (whichever comes first wins).
    UAbilityTask_WaitDelay* HitDelay = UAbilityTask_WaitDelay::WaitDelay(this, HitTime / FMath::Max(MontagePlayRate, 0.01f));
    HitDelay->OnFinish.AddDynamic(this, &UZeroBase_LightMelee::Strike);
    HitDelay->ReadyForActivation();
}

void UZeroBase_LightMelee::OnHitEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
    Strike();
}

void UZeroBase_LightMelee::Strike()
{
    if (bStruck)
    {
       return;
    }
    bStruck = true;

    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero || !HasAuthority(&CurrentActivationInfo))
    {
       return;
    }

    // Flat aim from the Mover input, so the server swings where the client looks.
    FVector Forward = GetHeroAimDirection();
    Forward.Z = 0.f;
    Forward = Forward.GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
       Forward = Hero->GetActorForwardVector();
    }

    const float CapsuleRadius = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f;
    const FVector Start = Hero->GetActorLocation();
    Hero->PerformMeleeStrike(Start, Start + Forward * (CapsuleRadius + StrikeRange), StrikeRadius, StrikeDamage);
}

void UZeroBase_LightMelee::OnMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData)
{
    // A montage shorter than HitTime still hits.
    Strike();
    FinishAbility(false);
}

void UZeroBase_LightMelee::OnMontageCancelled(FGameplayTag EventTag, FGameplayEventData EventData)
{
    FinishAbility(true);
}

void UZeroBase_LightMelee::FinishAbility(bool bWasCancelled)
{
    if (!IsActive())
    {
       return;
    }
    const bool bReplicateEnd = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEnd, bWasCancelled);
}
