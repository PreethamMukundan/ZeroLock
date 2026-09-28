// Copyright Preetham Mukundan (C) 2026

#include "Lash/ZL_Lash_DeathSlam.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Chaos/Deformable/MuscleActivationConstraints.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Lash/ZL_GATargetActor_CylinderCharge.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "Engine.h"
#include "GAS/Tasks/ZL_Task_WaitSlamLocation.h"

UZL_Lash_DeathSlam::UZL_Lash_DeathSlam()
{
}

void UZL_Lash_DeathSlam::TargetTimeOut()
{
    TargetInputRelease(0.01);
}

void UZL_Lash_DeathSlam::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo,
                                         const FGameplayAbilityActivationInfo ActivationInfo,
                                         const FGameplayEventData* TriggerEventData)
{
    FVector StartLocation = GetAvatarActorFromActorInfo()->GetActorLocation();
    FTransform SpawnTransform = FTransform::Identity;
    SpawnTransform.SetLocation(StartLocation);

    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero)
    {
       EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
       return;
    }

    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
       ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
    }

    InitWaitTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, 2);
    InitWaitTask->OnProgress.AddDynamic(this, &UZL_Lash_DeathSlam::UpdateProgressionTimer);
    InitWaitTask->OnStarted.AddDynamic(this, &UZL_Lash_DeathSlam::StartProgressionTimer);
    InitWaitTask->OnEnd.AddDynamic(this, &UZL_Lash_DeathSlam::StopProgressionTimer);
    InitWaitTask->OnFinished.AddDynamic(this, &UZL_Lash_DeathSlam::TargetTimeOut);
    InitWaitTask->ReadyForActivation();

    if (Hero->GetZeroMoverComponent())
    {
       //Hero->GetZeroMoverComponent()->RequestStopMovement();
       Hero->GetZeroMoverComponent()->QueueNextMode(FName("Locked"));
    }

    if (!Targetclass)
    {
       EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
       return;
    }

    AZL_GATargetActor_CylinderCharge* TargetActor = GetWorld()->SpawnActor<AZL_GATargetActor_CylinderCharge>(Targetclass, SpawnTransform);

    if (TargetActor)
    {
       TargetActor->OwningAbility = this;
       TargetActor->PrimaryPC = Cast<APlayerController>(ActorInfo->PlayerController.Get());
       TargetActor->SetOwner(GetAvatarActorFromActorInfo());

       TargetActor->RequiredTime = LockOnThreshold;
       TargetActor->CylinderRadius = CylinderRadius;
       TargetActor->CylinderHeight = CylinderLength;
       TargetActor->ReticleClass = ReticleClassX;

       FGameplayTargetDataFilter* Filter = new FGameplayTargetDataFilter();
       Filter->RequiredActorClass = AZeroLockCharacter::StaticClass();
       Filter->SelfFilter = ETargetDataFilterSelf::TDFS_NoSelf;
       TargetActor->Filter.Filter = TSharedPtr<FGameplayTargetDataFilter>(Filter);

       TargetActor->StartTargeting(this);

       if (TargetConfirmationStyle == EGASTargetConfirmationStyle::Instant)
       {
          WaitTargetTask = UAbilityTask_WaitTargetData::WaitTargetDataUsingActor(this, FName(TEXT("CylinderTargeting")), EGameplayTargetingConfirmation::Instant, TargetActor);
       }
       else
       {
          WaitTargetTask = UAbilityTask_WaitTargetData::WaitTargetDataUsingActor(this, FName(TEXT("CylinderTargeting")), EGameplayTargetingConfirmation::UserConfirmed, TargetActor);
       }

       WaitTargetTask->ValidData.AddDynamic(this, &UZL_Lash_DeathSlam::TargetConfirmed);
       WaitTargetTask->Cancelled.AddDynamic(this, &UZL_Lash_DeathSlam::TargetCancelled);
       WaitTargetTask->ReadyForActivation();

       if (TargetConfirmationStyle == EGASTargetConfirmationStyle::Quick)
       {
          UAbilityTask_WaitInputRelease* InputRTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
          InputRTask->OnRelease.AddDynamic(this, &UZL_Lash_DeathSlam::TargetInputRelease);
          InputRTask->ReadyForActivation();
       }
    }
    else
    {
       EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

void UZL_Lash_DeathSlam::AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data)
{
   if (InitWaitTask)
   {
      InitWaitTask->EndTask(); InitWaitTask = nullptr;
   }
    if (Data.Num() == 0 || !Data.Get(0))
    {
       EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
       return;
    }

    OutActors = Data.Get(0)->GetActors();

    ZLOG(FString::Printf(TEXT("Ability Confirmed! Locked on %d targets."), OutActors.Num()));
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());

    if (LockOnEffect)
    {
       FGameplayEffectContextHandle Context = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
       FGameplayEffectSpecHandle Spec = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(LockOnEffect, 1.f, Context);

       FVector TargetPullLocation = Hero ? (Hero->GetActorLocation() + FVector(0, 0, -45)) : FVector::ZeroVector;

       if (HasAuthority(&CurrentActivationInfo))
       {
           for (TWeakObjectPtr<AActor> Target : OutActors)
           {
              if (AZeroLockCharacter* Victim = Cast<AZeroLockCharacter>(Target.Get()))
              {
                 UAbilitySystemComponent* TargetASC = Victim->GetAbilitySystemComponent();
                 if (TargetASC)
                 {
                    FActiveGameplayEffectHandle ActiveHandle = Hero->GetMyAbilitySystemComp()->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
                    if (ActiveHandle.IsValid())
                    {
                       CurrentActiveEffectHandles.Add(ActiveHandle);
                    }

                    FVector VictimLoc = Victim->GetActorLocation();
                    FVector ToDestination = TargetPullLocation - VictimLoc;
                    FVector RequiredVelocity = ToDestination / PullTime;

                    FGameplayEventData Payload;
                    Payload.Instigator = Hero;
                    Payload.Target = Victim;
                    Payload.EventMagnitude = PullTime;

                    FGameplayAbilityTargetData_LocationInfo* LocData = new FGameplayAbilityTargetData_LocationInfo();
                    LocData->TargetLocation.LiteralTransform = FTransform(RequiredVelocity);
                    LocData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
                    Payload.TargetData.Add(LocData);

                    TargetASC->HandleGameplayEvent(FGameplayTag::RequestGameplayTag(FName("Zerolock.Movement.Forced")), &Payload);
                 }
              }
           }
       }
    }

    UAbilityTask_WaitDelay* WaitPullTask = UAbilityTask_WaitDelay::WaitDelay(this, PullTime);
    WaitPullTask->OnFinish.AddDynamic(this, &UZL_Lash_DeathSlam::TargetPullFinish);
    WaitPullTask->ReadyForActivation();
}

void UZL_Lash_DeathSlam::TargetPullFinish()
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
   for (TWeakObjectPtr<AActor> Target : OutActors)
   {
      if (AZeroLockCharacter* Victim = Cast<AZeroLockCharacter>(Target.Get()))
      {
         Victim->GetZeroMoverComponent()->RequestStopMovement();
      }
   }
    UZL_Task_WaitSlamLocation* SlamLocationTask = UZL_Task_WaitSlamLocation::FindSlamLocation(this, FName("SlamTask"),Hero,SlamRadius.GetValueAtLevel(GetCurrentAbilitySpec()->Level), 0.5f, 3000.f);
   if (SlamLocationTask)
   {
      SlamLocationTask->OnLocationReceived.AddDynamic(this, &UZL_Lash_DeathSlam::SlamLocationReceived);
      SlamLocationTask->ReadyForActivation();
   }
   else
   {
      EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
   }   
}

void UZL_Lash_DeathSlam::TargetSlamFinish()
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Lash_DeathSlam::SlamLocationReceived(const FVector& ExactSlamLocation)
{
   ZLOG("TargetLoction out")
   DrawDebugSphere(GetWorld(), ExactSlamLocation,32,32,FColor::Red,true);
    if (HasAuthority(&CurrentActivationInfo))
    {
        AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
        FVector TargetPullLocation = ExactSlamLocation;
        
        for (TWeakObjectPtr<AActor> Target : OutActors)
        {
           if (AZeroLockCharacter* Victim = Cast<AZeroLockCharacter>(Target.Get()))
           {
              UAbilitySystemComponent* TargetASC = Victim->GetAbilitySystemComponent();
              if (TargetASC)
              {
                 FVector VictimLoc = Victim->GetActorLocation();
                 FVector ToDestination = TargetPullLocation - VictimLoc;
                 FVector RequiredVelocity = ToDestination / PullTime;

                 FGameplayEventData Payload;
                 Payload.Instigator = Hero;
                 Payload.Target = Victim;
                 Payload.EventMagnitude = PullTime;

                 FGameplayAbilityTargetData_LocationInfo* LocData = new FGameplayAbilityTargetData_LocationInfo();
                 LocData->TargetLocation.LiteralTransform = FTransform(RequiredVelocity);
                 LocData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
                 Payload.TargetData.Add(LocData);

                 TargetASC->HandleGameplayEvent(FGameplayTag::RequestGameplayTag(FName("Zerolock.Movement.Forced")), &Payload);
              }
           }
        }
    }

    UAbilityTask_WaitDelay* WaitPullTask = UAbilityTask_WaitDelay::WaitDelay(this, PullTime);
    WaitPullTask->OnFinish.AddDynamic(this, &UZL_Lash_DeathSlam::TargetSlamFinish);
    WaitPullTask->ReadyForActivation();
}

void UZL_Lash_DeathSlam::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
   if (UBaseCharAbilitySystemComponent* BaseASC = Cast<UBaseCharAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
   {
      BaseASC->CachedSlamLocations.Remove(CurrentActivationInfo.GetActivationPredictionKey());
      BaseASC->UnregisterPendingSlamTask(CurrentActivationInfo.GetActivationPredictionKey());
   }
    for (FActiveGameplayEffectHandle& mHandle : CurrentActiveEffectHandles)
    {
       if (mHandle.IsValid())
       {
          UAbilitySystemComponent* TargetASC = mHandle.GetOwningAbilitySystemComponent();
          if (TargetASC)
          {
             TargetASC->RemoveActiveGameplayEffect(mHandle);
             
             if (Hero && HasAuthority(&CurrentActivationInfo))
             {
                Hero->GetMyAbilitySystemComp()->ApplySpiritDamage(TargetASC, SlamDamage.GetValueAtLevel(GetAbilityLevel()));
                if (AfterSlamEffect)
                {
                   Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(TargetASC, AfterSlamEffect, GetAbilityLevel());
                }
             }
          }
       }
    }

    if (Hero && Hero->GetZeroMoverComponent())
    {
       Hero->GetZeroMoverComponent()->QueueNextMode(FName("Falling"));
    }
    
    if (HasAuthority(&CurrentActivationInfo))
    {
        for (TWeakObjectPtr<AActor> Target : OutActors)
        {
           if (AZeroLockCharacter* Victim = Cast<AZeroLockCharacter>(Target.Get()))
           {
              if (UZeroMoverComponent* VictimMover = Victim->FindComponentByClass<UZeroMoverComponent>())
              {
                 if (VictimMover->GetMovementModeName() == FName("Locked"))
                 {
                    VictimMover->QueueNextMode(FName("Falling"));
                 }
              }
           }
        }
    }

    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
       ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

