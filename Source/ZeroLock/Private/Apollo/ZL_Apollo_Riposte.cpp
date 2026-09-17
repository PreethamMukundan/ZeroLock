// Copyright Preetham Mukundan (C) 2026

#include "Apollo/ZL_Apollo_Riposte.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Apollo/ZL_Apollo_FlawlessAdvance.h"
#include "Camera/CameraComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_Task_ApplyRootMotion.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/RootMotionSource.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/Tasks/ZL_AbilityTask_MoverMoveToActor.h"
#include "Mover/ZeroMoverComponent.h"

UZL_Apollo_Riposte::UZL_Apollo_Riposte()
{
}

void UZL_Apollo_Riposte::OnPlayerHit(AActor* HitActor)
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero)
    {
       EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
       return;
    }

    if (ResistanceEffect)
    {
       Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(Hero->GetMyAbilitySystemComp(), ResistanceEffect, GetAbilityLevel());
    }

    TArray<FOverlapResult> OverlapResults;
    float SphereRadius = 100.f;
    FVector SphereLocation = Hero->GetActorLocation() + (Hero->GetActorForwardVector() * 50);
    
    if (GetWorld()->OverlapMultiByChannel(OverlapResults, SphereLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(SphereRadius), Hero->GetIgnoreCharacterParams()))
    {
       for (const FOverlapResult& Overlap : OverlapResults)
       {
          if (AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor()))
          {
             if (Hero->IsOnSameTeam(Villan)) continue;
             if (StunEffect)
             {
                Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(Villan->GetMyAbilitySystemComp(), StunEffect, GetAbilityLevel());
             }
          }
       }
    }
    
    DrawDebugSphere(GetWorld(), SphereLocation, SphereRadius, 12, FColor::Red, false, 12.0f);
    EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false);
}





void UZL_Apollo_Riposte::OnInputPressed(float TimeWaited)
{
	ZLOG("input Pressed");
	
	if (WaitTargetTask->IsActive())
	{
		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
		if (Hero)
		{
			
			Hero->GetAbilitySystemComponent()->TargetConfirm();
		}
	}
}

void UZL_Apollo_Riposte::OnEventRecived(FGameplayEventData Payload)
{
    if (AnimMontageTask && AnimMontageTask->IsActive())
    {
       GetAbilitySystemComponentFromActorInfo()->CurrentMontageStop(-1);
       AnimMontageTask->EndTask();
    }
    if (WaitTimeTask && WaitTimeTask->IsActive())
    {
       WaitTimeTask->OnFinished.RemoveDynamic(this, &UZL_Apollo_Riposte::K2_EndAbility);
       WaitTimeTask->EndTask();
    }

    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	;
	
  
    if (!Hero || !TargetClass)
    {
       EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
       return;
    }

	if (Hero->GetZeroMoverComponent()->GetMovementModeName() == "Locked")
	{
		Hero->GetZeroMoverComponent()->QueueNextMode("Falling");
	}
	
    FTransform SpawnTransform = FTransform::Identity;
    SpawnTransform.SetLocation(Hero->GetActorLocation());

    AGameplayAbilityTargetActor_Trace* TargetActor = GetWorld()->SpawnActor<AGameplayAbilityTargetActor_Trace>(TargetClass, SpawnTransform);
    if (!TargetActor) return;

    TargetActor->OwningAbility = this;
    TargetActor->PrimaryPC = Cast<APlayerController>(Hero->GetController());
    TargetActor->SetOwner(Hero);
    
    TargetActor->StartLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
    TargetActor->StartLocation.LiteralTransform = SpawnTransform;

    TSharedPtr<FGameplayTargetDataFilter> TargetFilter = MakeShared<FGameplayTargetDataFilter>();
    TargetFilter->RequiredActorClass = AZeroLockCharacter::StaticClass();
    TargetFilter->SelfFilter = ETargetDataFilterSelf::TDFS_NoSelf;
    TargetFilter->SelfActor = Hero; 

    TargetActor->Filter = FGameplayTargetDataFilterHandle(TargetFilter);
    TargetActor->StartTargeting(this);
    TargetActor->ReticleClass = ReticleClassX;
	TargetActor->MaxRange =100000;

    WaitTargetTask = UAbilityTask_WaitTargetData::WaitTargetDataUsingActor(
        this, FName(TEXT("RiposteTargeting")), EGameplayTargetingConfirmation::UserConfirmed, TargetActor);

    if (WaitTargetTask)
    {
        WaitTargetTask->ValidData.AddDynamic(this, &UZL_Apollo_Riposte::OnTargetConfirmed);
        WaitTargetTask->Cancelled.AddDynamic(this, &UZL_Apollo_Riposte::OnTargetCancelled);
        WaitTargetTask->ReadyForActivation();
    }
	ZLOG("Hit Detected");
	InputPressedTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	InputPressedTask->OnPress.AddDynamic(this, &UZL_Apollo_Riposte::OnInputPressed);
	InputPressedTask->ReadyForActivation();
	WaitTimeTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this,MaxTime);
	WaitTimeTask->OnProgress.AddDynamic(this,&UZL_Apollo_Riposte::UpdateProgressionTimer);
	WaitTimeTask->OnStarted.AddDynamic(this,&UZL_Apollo_Riposte::StartProgressionTimer);
	WaitTimeTask->OnEnd.AddDynamic(this,&UZL_Apollo_Riposte::StopProgressionTimer);
	WaitTimeTask->OnFinished.AddDynamic(this,&UZL_Apollo_Riposte::K2_EndAbility);
	WaitTimeTask->ReadyForActivation();
}

void UZL_Apollo_Riposte::OnTargetReached()
{
	if (!OutActors.IsEmpty())
	{
		if (OutActors[0].Get())
		{
			OnPlayerHit(OutActors[0].Get());
			return;
		}
		else
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	
	
}

void UZL_Apollo_Riposte::OnTargetConfirmed(const FGameplayAbilityTargetDataHandle& Data)
{
	if (WaitTimeTask && WaitTimeTask->IsActive())
	{
		WaitTimeTask->OnFinished.RemoveDynamic(this, &UZL_Apollo_Riposte::K2_EndAbility);
		WaitTimeTask->EndTask();
	}
	if (InputPressedTask && InputPressedTask->IsActive())
	{
		InputPressedTask->OnPress.RemoveDynamic(this, &UZL_Apollo_Riposte::OnInputPressed);
		InputPressedTask->EndTask();
	}
	
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
        return;
    }

	if (Data.Num() == 0 || !Data.Get(0))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	OutActors = Data.Get(0)->GetActors();

	if (!OutActors.IsEmpty())
	{
		if (OutActors[0].Get())
		{
			UZL_AbilityTask_MoverMoveToActor* MoveToActorTask = UZL_AbilityTask_MoverMoveToActor::ApplyMoverMoveToActor(this,FName("MoveaToActor"),OutActors[0].Get(),1.0f,50);
			MoveToActorTask->OnTargetReached.AddDynamic(this,&UZL_Apollo_Riposte::OnTargetReached);
			MoveToActorTask->ReadyForActivation();
		}
		else
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UZL_Apollo_Riposte::OnTargetCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UZL_Apollo_Riposte::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (Hero && ParryEffect)
    {
    	Hero->GetZeroMoverComponent()->RequestStopMovement();
    	Hero->GetZeroMoverComponent()->QueueNextMode("Locked");
       Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(Hero->GetMyAbilitySystemComp(),ParryEffect,GetAbilityLevel());
    }
    if (ParryAnimMontage)
    {
       AnimMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,FName("AnimMontageAndWait"),ParryAnimMontage);
       AnimMontageTask->ReadyForActivation();
    }
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
       ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
    }

    WaitTimeTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this,MaxTime);
    WaitTimeTask->OnProgress.AddDynamic(this,&UZL_Apollo_Riposte::UpdateProgressionTimer);
    WaitTimeTask->OnStarted.AddDynamic(this,&UZL_Apollo_Riposte::StartProgressionTimer);
    WaitTimeTask->OnEnd.AddDynamic(this,&UZL_Apollo_Riposte::StopProgressionTimer);
    WaitTimeTask->OnFinished.AddDynamic(this,&UZL_Apollo_Riposte::K2_EndAbility);
    WaitTimeTask->ReadyForActivation();
    
    UAbilityTask_WaitGameplayEvent* WaitGameplayEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,FGameplayTag::RequestGameplayTag("Event.UntouchableTrigger",false));
    WaitGameplayEvent->EventReceived.AddDynamic(this,&UZL_Apollo_Riposte::OnEventRecived);
    WaitGameplayEvent->ReadyForActivation();
}

void UZL_Apollo_Riposte::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    CommitAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo());
    
    if (AnimMontageTask && AnimMontageTask->IsActive())
    {
       GetAbilitySystemComponentFromActorInfo()->CurrentMontageStop(-1);
       AnimMontageTask->EndTask();
    }
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
       ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
    }
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero )
	{
		if (Hero->GetZeroMoverComponent()->GetMovementModeName() == "Locked")
		{
			Hero->GetZeroMoverComponent()->QueueNextMode("Falling");
		}
		
	}
    FGameplayTag UntouchableTag = FGameplayTag::RequestGameplayTag(FName("Zerolock.Untouchable"));
    FGameplayTagContainer TagContainer;
    TagContainer.AddTag(UntouchableTag);

    GetAbilitySystemComponentFromActorInfo()->RemoveActiveEffectsWithGrantedTags(TagContainer);
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

