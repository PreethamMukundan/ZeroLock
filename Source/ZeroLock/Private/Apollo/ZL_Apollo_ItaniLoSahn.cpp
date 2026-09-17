// Copyright Preetham Mukundan (C) 2026


#include "Apollo/ZL_Apollo_ItaniLoSahn.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/Tasks/ZL_AbilityTask_MoverMoveTo.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Apollo_ItaniLoSahn::UZL_Apollo_ItaniLoSahn()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UZL_Apollo_ItaniLoSahn::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (Hero)
    {
      Hero->GetZeroMoverComponent()->RequestStopMovement();
        Hero->GetZeroMoverComponent()->QueueNextMode("Locked");
        HeightSave= Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        RadiusSave= Hero->GetCapsuleComponent()->GetScaledCapsuleRadius();
    }

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
    }

    WaitTimeTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, ChargeupTime);
    WaitTimeTask->OnProgress.AddDynamic(this,&UZL_Apollo_ItaniLoSahn::UpdateProgressionTimer);
    WaitTimeTask->OnStarted.AddDynamic(this,&UZL_Apollo_ItaniLoSahn::StartProgressionTimer);
    WaitTimeTask->OnEnd.AddDynamic(this,&UZL_Apollo_ItaniLoSahn::StopProgressionTimer);
    WaitTimeTask->OnFinished.AddDynamic(this, &UZL_Apollo_ItaniLoSahn::ChargedDone);
    WaitTimeTask->ReadyForActivation();
}

void UZL_Apollo_ItaniLoSahn::ChargedDone()
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero) return;
    DashDetectionSphere = NewObject<USphereComponent>(Hero, TEXT("DashDetectionSphere"));
    DashDetectionSphere->RegisterComponent();
    DashDetectionSphere->SetSphereRadius(DamageRadius);
    DashDetectionSphere->AttachToComponent(Hero->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);
    DashDetectionSphere->SetHiddenInGame(false);
    DashDetectionSphere->SetVisibility(true);
    DashDetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DashDetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DashDetectionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
    UAbilityTask_WaitInputRelease* InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    InputReleaseTask->OnRelease.AddDynamic(this, &UZL_Apollo_ItaniLoSahn::ReleaseInputRelease);
    InputReleaseTask->ReadyForActivation();
}

void UZL_Apollo_ItaniLoSahn::ReleaseInputRelease(float TimeHeld)
{
    CommitAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo());
	
   AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero) return;

    Hero->GetCapsuleComponent()->SetCapsuleHalfHeight(0.1);
    Hero->GetCapsuleComponent()->SetCapsuleRadius(0.1);
    Hero->GetCameraBoom()->bEnableCameraLag = true;
    Hero->GetCameraBoom()->CameraLagSpeed = 0.01f;

    FVector CameraLoc; FRotator CameraRot;
    Hero->GetActorEyesViewPoint(CameraLoc, CameraRot);
    FVector ForwardDir = CameraRot.Vector();

    FVector TraceStart = Hero->GetActorLocation();
    FVector TraceEnd = TraceStart + (ForwardDir * MaxTargetRange.GetValueAtLevel(GetAbilityLevel()));
    
    TArray<AActor*> IgnoreList;
    IgnoreList.Add(Hero);
    
    FHitResult DashHit;
    UKismetSystemLibrary::CapsuleTraceSingle(this, TraceStart, TraceEnd, 0.1, 0.1, UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreList, EDrawDebugTrace::ForDuration, DashHit, true);

    FVector FinalTarget = TraceEnd;

    CurrentTargetData.Clear();
    
    TArray<FHitResult> OutHits;

    FCollisionShape SweepSphere = FCollisionShape::MakeSphere(DamageRadius); 
    
    UniqueHitActors.Empty();
    DashDetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &UZL_Apollo_ItaniLoSahn::OnDashSphereOverlap);
    TArray<AActor*> InitialOverlaps;
    DashDetectionSphere->GetOverlappingActors(InitialOverlaps, AZeroLockCharacter::StaticClass());

    for (AActor* OverlappedActor : InitialOverlaps)
    {
        OnDashSphereOverlap(DashDetectionSphere, OverlappedActor, nullptr, 0, false, FHitResult());
    }
    if (IsLocallyControlled())
    {
        GetAbilitySystemComponentFromActorInfo()->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), CurrentTargetData, FGameplayTag(), FPredictionKey());
    }

    MoveToTask =UZL_AbilityTask_MoverMoveTo::ApplyMoverMoveTo(this,"MoveToTask",TraceStart,FinalTarget,0.25,true);
    MoveToTask->OnFinished.AddDynamic(this, &UZL_Apollo_ItaniLoSahn::OnMoveToDone);
    MoveToTask->ReadyForActivation();
    
    TempWaitTask = UAbilityTask_WaitDelay::WaitDelay(this,0.3);
    TempWaitTask->OnFinish.AddDynamic(this,&UZL_Apollo_ItaniLoSahn::OnMoveToDone);
    TempWaitTask->ReadyForActivation();
    
}

void UZL_Apollo_ItaniLoSahn::OnMoveToDone()
{
    if (TempWaitTask && TempWaitTask->IsActive())
    {
        TempWaitTask->OnFinish.RemoveDynamic(this, &UZL_Apollo_ItaniLoSahn::OnMoveToDone);
        TempWaitTask->EndTask();
    }
    if (MoveToTask && MoveToTask->IsActive())
    {
        MoveToTask->OnFinished.RemoveDynamic(this, &UZL_Apollo_ItaniLoSahn::OnMoveToDone);
        MoveToTask->EndTask();
    }
    UAbilityTask_WaitDelay* WaitDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, 1.8f);
    WaitDelayTask->OnFinish.AddDynamic(this, &UZL_Apollo_ItaniLoSahn::OnExecutecallback);
    WaitDelayTask->ReadyForActivation();
  
}

void UZL_Apollo_ItaniLoSahn::OnTimeFinish()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

}

void UZL_Apollo_ItaniLoSahn::OnExecutecallback()
{
    UAbilityTask_WaitDelay* WaitDelayTaskZ = UAbilityTask_WaitDelay::WaitDelay(this, 1.2f);
    WaitDelayTaskZ->OnFinish.AddDynamic(this, &UZL_Apollo_ItaniLoSahn::OnTimeFinish);
    WaitDelayTaskZ->ReadyForActivation();
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero) return;
    Hero->GetCameraBoom()->CameraLagSpeed = 10.0f;
    if (!HasAuthority(&CurrentActivationInfo)) return;
    
    for (int32 i = 0; i < CurrentTargetData.Num(); i++)
    {
        TArray<TWeakObjectPtr<AActor>> TargetActors = CurrentTargetData.Get(i)->GetActors();
        for (auto& ActorPtr : TargetActors)
        {
            if (AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(ActorPtr.Get()))
            {
                float DamageAmp = Damage.GetValueAtLevel(GetAbilityLevel());
                
                if (Villan->GetMyAttributeSet())
                {
                    float HealthPercent = Villan->GetMyAttributeSet()->GetCurrentHealth() / Villan->GetMyAttributeSet()->GetMaximumHealth();
                    if (HealthPercent < 0.5f)
                    {
                        DamageAmp  = DamageAmp + ((BonusDamagePercent.GetValueAtLevel(GetAbilityLevel())/100)*DamageAmp);
                    }
                }
                Villan->CustomTimeDilation = 1.0f;
                Villan->GetAbilitySystemComponent()->RemoveGameplayCue(FGameplayTag::RequestGameplayTag("GameplayCue.Apollo.ItaniLoSahn",false));
                   
                
                if (UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp())
                {
                    HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), DamageAmp);
                }
            }
        }
    }

}

void UZL_Apollo_ItaniLoSahn::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
   // CommitAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo());
	
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (Hero)
    {
        Hero->GetZeroMoverComponent()->QueueNextMode("Falling");
        Hero->GetCameraBoom()->bEnableCameraLag = false;
        Hero->GetCameraBoom()->CameraLagSpeed = 10.0f;
        Hero->GetCapsuleComponent()->SetCapsuleHalfHeight(HeightSave);
        Hero->GetCapsuleComponent()->SetCapsuleRadius(RadiusSave);
    }
    if (DashDetectionSphere)
    {
        DashDetectionSphere->OnComponentBeginOverlap.RemoveDynamic(this, &UZL_Apollo_ItaniLoSahn::OnDashSphereOverlap);
        DashDetectionSphere->DestroyComponent();
        DashDetectionSphere = nullptr;
    }

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
    }

    CurrentTargetData.Clear();
    ZLOG("End Ability itani");
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZL_Apollo_ItaniLoSahn::OnDashSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(OtherActor);

    if (Hero && Villan && !UniqueHitActors.Contains(Villan))
    {
        if (!Hero->IsOnSameTeam(Villan))
        {
            UniqueHitActors.Add(Villan);
            
            FGameplayAbilityTargetData_SingleTargetHit* NewData = new FGameplayAbilityTargetData_SingleTargetHit();

            NewData->HitResult.HitObjectHandle = FActorInstanceHandle(Villan);
            CurrentTargetData.Add(NewData);

            Villan->GetAbilitySystemComponent()->AddGameplayCue(FGameplayTag::RequestGameplayTag("GameplayCue.Apollo.ItaniLoSahn"));
            Villan->CustomTimeDilation = 0.05f;
        }
    }
    if (IsLocallyControlled())
    {
        GetAbilitySystemComponentFromActorInfo()->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), CurrentTargetData, FGameplayTag(), FPredictionKey());
    }
}
