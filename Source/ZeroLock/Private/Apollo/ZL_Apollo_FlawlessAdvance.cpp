// Copyright Preetham Mukundan (C) 2026


#include "Apollo/ZL_Apollo_FlawlessAdvance.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_AbilityTask_MoverMoveTo.h"
#include "GAS/Tasks/ZL_WaitChargeRelease_Task.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Mover/ZeroMovementData.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Apollo_FlawlessAdvance::UZL_Apollo_FlawlessAdvance()
{
}



void UZL_Apollo_FlawlessAdvance::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                 const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                 const FGameplayEventData* TriggerEventData)
{
	CurrentLungeCount = 0;
	StartChargePhase();
}

void UZL_Apollo_FlawlessAdvance::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	CommitAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo());
	
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
	if (AnimMontageTask && AnimMontageTask->IsActive())
	{
		AnimMontageTask->EndTask();
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZL_Apollo_FlawlessAdvance::OnInitialMoveComplete()
{
	if (ActiveChargeMovementTask && ActiveChargeMovementTask->IsActive())
	{
		ActiveChargeMovementTask->EndTask();
	}
	ChargeStartTime = GetWorld()->GetTimeSeconds();
	UZL_WaitChargeRelease_Task* ZLWaitRelease = UZL_WaitChargeRelease_Task::WaitChargeRelease(this,nullptr,MaxLungeDuration,PerfectWindowMin,PerfectWindowMax);
	if (ZLWaitRelease)
	{
		if (GetCurrentActorInfo()->IsLocallyControlled())
		{
			ZLWaitRelease->OnInit.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::StartChargePhaseUI);
			ZLWaitRelease->OnProgressUpdate.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::UpdateChargePhaseUI);
			ZLWaitRelease->OnEnd.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::RemoveChargePhaseUI);
		}
		
		ZLWaitRelease->OnReleased.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::OnChargeReleased);
		ZLWaitRelease->ReadyForActivation();
	}
}

void UZL_Apollo_FlawlessAdvance::StartChargePhase()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
    
	if (!Hero) return;
    
	if (AnimMontageTask && AnimMontageTask->IsActive())
	{
		AnimMontageTask->EndTask();
	}
	AnimMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, FName("AnimMontageAndWait"), ChargeMontage);
	AnimMontageTask->ReadyForActivation();
    
	FVector MoveDir = Hero->GetInputWorldDir();
	FVector StartLoc = Hero->GetActorLocation();
	FVector TargetLoc = StartLoc + (MoveDir.GetSafeNormal() * ChargeVelocity);
	
	ActiveChargeMovementTask = UZL_AbilityTask_MoverMoveTo::ApplyMoverMoveTo(this, FName("InitChargeTask"), StartLoc, TargetLoc, ChargeTime,true);
	ActiveChargeMovementTask->OnFinished.AddDynamic(this, &UZL_Apollo_FlawlessAdvance::OnInitialMoveComplete);
	ActiveChargeMovementTask->ReadyForActivation();
}
FVector UZL_Apollo_FlawlessAdvance::GetExactInputDirection() const
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero)
	{
		return FVector::ZeroVector;
	}

	UZeroMoverComponent* MoverComp = Hero->GetZeroMoverComponent();
	if (!MoverComp)
	{
		return FVector::ZeroVector;
	}

	if (Hero->IsLocallyControlled())
	{
		FRotator ControlRot = Hero->GetControlRotation();
		FRotator YawRotation(0.0f, ControlRot.Yaw, 0.0f);

		FVector LocalInput(Hero->GetSyncedInput().Y, Hero->GetSyncedInput().X, 0.0f);
		FVector WorldDirection = ControlRot.RotateVector(LocalInput);
		
		return WorldDirection.GetSafeNormal();
	}
 
	if (Hero->HasAuthority())
	{
		const FMoverSyncState& SyncState = MoverComp->GetSyncState();
		if (const FCharacterDefaultInputs* DefaultInputs = SyncState.SyncStateCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			FVector SyncedMoveDir = DefaultInputs->GetMoveInput();
			return SyncedMoveDir;
		}
	}

	return FVector::ZeroVector;
}

void UZL_Apollo_FlawlessAdvance::OnRelease(float TimeHeld)
{
	ZLOG("Released");
	float CleanHoldTime = GetWorld()->GetTimeSeconds() - ChargeStartTime;
	bool bIsPerfect = (CleanHoldTime >= PerfectWindowMin && CleanHoldTime <= PerfectWindowMax);
	

	ExecuteLunge(bIsPerfect);
}

void UZL_Apollo_FlawlessAdvance::OnChargeReleased(float TotalTime, bool bWasPerfect)
{
	ZLOG("Released");
	ExecuteLunge(bWasPerfect);
}

void UZL_Apollo_FlawlessAdvance::ExecuteLunge(bool bIsPerfect)
{
	if (ActiveChargeMovementTask && ActiveChargeMovementTask->IsActive())
	{
		ActiveChargeMovementTask->EndTask();
	}
    
	AZeroLockCharacter* Character = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;

	UZeroMoverComponent* MoverComp = Character->FindComponentByClass<UZeroMoverComponent>();
	FVector LookDir = Character->GetActorForwardVector();
	if (Character->IsLocallyControlled() && Character->GetController())
	{
		LookDir = Character->GetController()->GetControlRotation().Vector();
	}
	else if (Character->HasAuthority() && MoverComp)
	{
		const FMoverSyncState& SyncState = MoverComp->GetSyncState();
		
		if (const FZeroMovementInputs* ZeroInputs = SyncState.SyncStateCollection.FindDataByType<FZeroMovementInputs>())
		{
			LookDir = ZeroInputs->LookDir.Vector();
		}
	}
	float FinalVelocity = bIsPerfect ? LungeBurstVelocity.GetValueAtLevel(GetAbilityLevel()) * 1.5f : LungeBurstVelocity.GetValueAtLevel(GetAbilityLevel());
	float CalculatedDistance = (FinalVelocity * 0.2f) + 500.0f;

	FVector StartLocation = Character->GetActorLocation();
	FVector TargetLocation = StartLocation + (LookDir * CalculatedDistance);
    
    TArray<FHitResult> Hits;
    TArray<AZeroLockCharacter*> Targets;
    TArray<AActor*> Ignored;
    Ignored.Add(Character);
    float DamageToUse  = bIsPerfect ? PerfectDamage.GetValueAtLevel(GetAbilityLevel()) : BaseDamageValue.GetValueAtLevel(GetAbilityLevel());

    if (ReverseConeTraceMulti(GetWorld(), GetAvatarActorFromActorInfo()->GetActorLocation(), LookDir.Rotation(), CalculatedDistance, 10.0f, UEngineTypes::ConvertToTraceType(ECC_Pawn), false, Ignored, EDrawDebugTrace::ForDuration, Hits, Targets, true, FLinearColor::Green, FLinearColor::Red, 1.5f))
    {
        if (Targets.Num() > 0)
        {
            for (AZeroLockCharacter* villan : Targets)
            {
                Character->GetMyAbilitySystemComp()->ApplySpiritDamage(villan->GetMyAbilitySystemComp(), DamageToUse);
            }
            if (bIsPerfect)
            {
                Character->GetMyAbilitySystemComp()->ApplyHeal(Character->GetMyAbilitySystemComp(), HealValue.GetValueAtLevel(GetAbilityLevel()));
            }
        }
    }
    
	LungeRootMotionTask = UZL_AbilityTask_MoverMoveTo::ApplyMoverMoveTo(this, FName("LungeTask"), StartLocation, TargetLocation, ChargeTime,true);
	LungeRootMotionTask->OnFinished.AddDynamic(this, &UZL_Apollo_FlawlessAdvance::OnLungeFinished);
	LungeRootMotionTask->ReadyForActivation();
}

void UZL_Apollo_FlawlessAdvance::OnLungeFinished()
{
	if (LungeRootMotionTask && LungeRootMotionTask->IsActive())
	{
		LungeRootMotionTask->EndTask();
	}
	CurrentLungeCount++;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
	if (AnimMontageTask && AnimMontageTask->IsActive())
	{
		AnimMontageTask->EndTask();
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	//if (Hero && Hero->GetCharacterMovement())
	//{
		//Hero->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	//}
	if (CurrentLungeCount < MaxLunges)
	{ 
		UAbilityTask_WaitInputPress* WaitPress = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
		WaitPress->OnPress.AddDynamic(this, &UZL_Apollo_FlawlessAdvance::OnRecastPressed);
		WaitPress->ReadyForActivation();
		if (WaitTimeTask && WaitTimeTask->IsActive())
		{
			WaitTimeTask->OnFinished.RemoveDynamic(this, &UZL_Apollo_FlawlessAdvance::K2_EndAbility);
			WaitTimeTask->EndTask();
		}
		WaitTimeTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this,RecastWindow);
		WaitTimeTask->OnProgress.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::UpdateProgressionTimer);
		WaitTimeTask->OnStarted.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::StartProgressionTimer);
		WaitTimeTask->OnEnd.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::StopProgressionTimer);
		WaitTimeTask->OnFinished.AddDynamic(this,&UZL_Apollo_FlawlessAdvance::K2_EndAbility);
		WaitTimeTask->ReadyForActivation();
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UZL_Apollo_FlawlessAdvance::OnRecastPressed(float TimeWaited)
{
	StartChargePhase();
}

void UZL_Apollo_FlawlessAdvance::DetectLungeTargets(const FVector& StartLocation, const FVector& Direction,
	float Distance, bool bIsPerfect)
{
}
