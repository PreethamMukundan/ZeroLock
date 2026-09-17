// Copyright Preetham Mukundan (C) 2026


#include "Lash/ZL_Lash_GroundStrike.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Engine/OverlapResult.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_AbilityTask_MoverMoveTo.h"
#include "GAS/Tasks/ZL_AbilityTask_Reach_MoverMoveTo.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"


UZL_Lash_GroundStrike::UZL_Lash_GroundStrike()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UZL_Lash_GroundStrike::GetLookAtLocation(const AZeroLockCharacter* InActor, float BaseRadius, float HeightMultiplier, float MaxAllowedRadius, FHitResult& OutHit)
{
	if (!InActor || !InActor->GetWorld()) return false;
	UWorld* World = InActor->GetWorld();
	FVector ActorLoc = InActor->GetActorLocation();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(InActor);
	
	FHitResult HeightCheckHit;
	float Height = 0.f;
	if (World->LineTraceSingleByChannel(HeightCheckHit, ActorLoc - FVector(0,0,45), ActorLoc + (FVector::DownVector * 10000.f), ECC_Vehicle, Params))
	{
		Height = FVector::Dist(ActorLoc, HeightCheckHit.ImpactPoint);
	}
	

	float DynamicRadius = FMath::Clamp(BaseRadius + (Height * HeightMultiplier), BaseRadius, MaxAllowedRadius);
	FVector CenterBase = HeightCheckHit.bBlockingHit ? HeightCheckHit.ImpactPoint : ActorLoc;


	FVector Start = InActor->GetPawnViewLocation();
	FVector End = Start + (InActor->GetViewRotation().Vector() * 10000.f);
	FHitResult LookHit;

	if (World->LineTraceSingleByChannel(LookHit, Start, End, ECC_Visibility, Params))
	{
		FVector ToHitFlat = LookHit.ImpactPoint - CenterBase;
		ToHitFlat.Z = 0;
		
		if (ToHitFlat.Size() > DynamicRadius)
		{
			FVector ClampedPos = CenterBase + (ToHitFlat.GetSafeNormal() * DynamicRadius);
			return World->LineTraceSingleByChannel(OutHit, ClampedPos + FVector(0,0,500), ClampedPos + (FVector::DownVector * 1000.f), ECC_Visibility, Params);
		}
		OutHit = LookHit;
		return true;
	}

	// Fallback to ground center
	OutHit = HeightCheckHit;
	return HeightCheckHit.bBlockingHit;
}

void UZL_Lash_GroundStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(ActorInfo->AvatarActor.Get());
	FHitResult TargetHit;

	if (GetLookAtLocation(Hero, BaseRadius, HeightScaleMultiplier, MaxAllowedRadius, TargetHit))
	{
		CurrentDashDestination = TargetHit.ImpactPoint;
		DistanceTravelled = FVector::Dist(Hero->GetActorLocation(), CurrentDashDestination);
		CaughtVictims.Empty();
		CaughtVictims.Add(Hero);
		
		
		FGameplayCueParameters LandCueParam;
		LandCueParam.Location = CurrentDashDestination;
		LandCueParam.Instigator = Hero;
		
		Hero->GetAbilitySystemComponent()->AddGameplayCue(FGameplayTag::RequestGameplayTag(FName("GameplayCue.Lash.GroundStrike"),false),LandCueParam);

		FVector OffSetOfSphere = Hero->GetActorForwardVector().GetSafeNormal() * 100 ;
		ActiveDragSphere = NewObject<USphereComponent>(Hero);
		ActiveDragSphere->SetSphereRadius(250.f);
		ActiveDragSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		ActiveDragSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
		ActiveDragSphere->SetupAttachment(Hero->GetRootComponent());
		ActiveDragSphere->RegisterComponent();
		ActiveDragSphere->SetVisibility(true);
		ActiveDragSphere->SetHiddenInGame(false);
		ActiveDragSphere->SetRelativeLocation(OffSetOfSphere);
		ActiveDragSphere->OnComponentBeginOverlap.AddDynamic(this, &UZL_Lash_GroundStrike::OnVictimCaught);

		if (SlamMontage)
		{
			GetOwnerASC()->PlayMontage(this,GetCurrentActivationInfo(),SlamMontage,1);
			
		}
		FVector StartLoc = Hero->GetActorLocation();
		FVector MoveDir = (CurrentDashDestination - StartLoc).GetSafeNormal();
		float Distance = FVector::Dist(StartLoc, CurrentDashDestination);
		float Duration = (MovementSpeed > 0) ? (Distance / MovementSpeed) : 0.1f;
		
		UZL_AbilityTask_Reach_MoverMoveTo* MovementTask = UZL_AbilityTask_Reach_MoverMoveTo::ApplyMoverReachMoveTo(this, FName("LaunchTask"), StartLoc, CurrentDashDestination, Duration,50,Distance *4,true);
		MovementTask->OnFinished.AddDynamic(this, &UZL_Lash_GroundStrike::OnMovementFinished);
		MovementTask->ReadyForActivation();

/*
		UAbilityTask_ApplyRootMotionConstantForce* MoveTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this, FName("LashMove"), MoveDir, MovementSpeed, Duration, false, nullptr,
			ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, false
		);

		MoveTask->OnFinish.AddDynamic(this, &UZL_Lash_GroundStrike::OnMovementFinished);
		MoveTask->ReadyForActivation();*/
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UZL_Lash_GroundStrike::OnVictimCaught(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	if (OtherActor && !CaughtVictims.Contains(OtherActor))
    {
        AZeroLockCharacter* Victim = Cast<AZeroLockCharacter>(OtherActor);
        AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
		if (Hero->IsOnSameTeam(Victim)) return;
        if (Victim && Hero && Victim->GetAbilitySystemComponent())
        {
            CaughtVictims.Add(Victim);

            FVector VictimLoc = Victim->GetActorLocation();
            FVector PlayerLoc = Hero->GetActorLocation();
            
            FVector HorizontalOffset = VictimLoc - PlayerLoc;
            HorizontalOffset.Z = 0; 
            

            FVector PersonalDestination = CurrentDashDestination + HorizontalOffset;


            FVector ToDestination = PersonalDestination - VictimLoc;
            float Distance = ToDestination.Size();
            float SyncDuration = Distance / MovementSpeed;


            if (StunEffectClass)
            {
                FGameplayEffectSpecHandle Spec = Victim->GetAbilitySystemComponent()->MakeOutgoingSpec(StunEffectClass, 1.f, Victim->GetAbilitySystemComponent()->MakeEffectContext());
                if (Spec.IsValid()) 
                { 
                    Spec.Data->Duration = SyncDuration; 
                    VictimStunHandles.Add(Victim->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*Spec.Data)); 
                }
            }
        	
            if (Hero->HasAuthority())
            {
            	if (UZeroMoverComponent* VictimMover = Victim->FindComponentByClass<UZeroMoverComponent>())
            	{
            		// Calculate exact velocity to reach the personal destination
            		FVector DragVelocity = ToDestination.GetSafeNormal() * MovementSpeed;
            		VictimMover->RequestSafeAbilityMove(ToDestination, SyncDuration);
            	}
            }
        }
    }
}

void UZL_Lash_GroundStrike::OnMovementFinished(bool didreach)
{

	ZLOG("MovementDone")
	if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (GetCurrentActorInfo()->IsNetAuthority())
		{
			
		UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
		if (ASC)
		{
			ASC->StopMontageIfCurrent(*SlamMontage,1);
		}
		TArray<FOverlapResult> OverlapResults;
		FVector Origin = Hero->GetActorLocation();
		FVector Direction = Hero->GetActorForwardVector()-FVector(0,0,0.5);
		TArray<FHitResult> HitResults;
		TArray<AActor*> ActorsToIgnore;
		TArray<AZeroLockCharacter*> OutVillans;
		ActorsToIgnore.Add(Hero);
		int32 mylevel =GetCurrentAbilitySpec()->Level;
		float DamageCalc =StrikeBaseDam.GetValueAtLevel(mylevel) + (DistanceTravelled* StrikeDistanceMultipler.GetValueAtLevel(mylevel));
		if (ConeTraceMulti(GetWorld(),Origin,Direction.Rotation(),StrikeConeHeight,StrikeConeAngle/2, UEngineTypes::ConvertToTraceType(ECC_Pawn),true,ActorsToIgnore,EDrawDebugTrace::ForDuration,HitResults,OutVillans,true))
		{
			if (OutVillans.Num() > 0)
			{
				for (AZeroLockCharacter* villan : OutVillans)
				{
					if (!villan) continue;
					if (Hero->IsOnSameTeam(villan)) continue;
					ZLOG(FString::SanitizeFloat(DamageCalc));
					Hero->GetMyAbilitySystemComp()->ApplySpiritDamage(villan->GetMyAbilitySystemComp(),DamageCalc);
					if (mylevel>=2)
					{
				//		villan->LaunchCharacter(FVector(0,0,100),true,true);
						if (KnockUpEffect)
						{
							Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(villan->GetMyAbilitySystemComp(),KnockUpEffect,mylevel);
						}
					}
				}
			}
				
			
		}
	
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		
	}
	

}

void UZL_Lash_GroundStrike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	for (FActiveGameplayEffectHandle& StunHandle : VictimStunHandles)
	{
		if (StunHandle.IsValid())
		{
			StunHandle.GetOwningAbilitySystemComponent()->RemoveActiveGameplayEffect(StunHandle);
		}
	}
	VictimStunHandles.Empty();

	GetAbilitySystemComponentFromActorInfo()->RemoveGameplayCue(FGameplayTag::RequestGameplayTag(FName("GameplayCue.Lash.GroundStrike"),false));
	if (ActiveDragSphere)
	{
		ActiveDragSphere->DestroyComponent();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
