// Copyright Preetham Mukundan (C) 2026


#include "Lash/ZL_Lash_Grapple.h"

#include "Camera/CameraComponent.h"
#include "Engine/OverlapResult.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_AbilityTask_MoverMoveTo.h"
#include "ZeroLock/ZeroLockCharacter.h"


UZL_Lash_Grapple::UZL_Lash_Grapple()
{
}

void UZL_Lash_Grapple::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero)
	{
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
	}
	
	TArray<FOverlapResult> OverlapResults;
	FVector Origin = Hero->GetPawnViewLocation();
	FVector Direction = Hero->GetFollowCamera()->GetForwardVector();
	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore;
	TArray<AZeroLockCharacter*> OutVillans;
	ActorsToIgnore.Add(Hero);
	int32 mylevel =GetCurrentAbilitySpec()->Level;
	//CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	if (ConeTraceMulti(GetWorld(),Origin,Direction.Rotation(),ConeHeight.GetValueAtLevel(mylevel),ConeAngle/2, UEngineTypes::ConvertToTraceType(ECC_Pawn),true,ActorsToIgnore,EDrawDebugTrace::None,HitResults,OutVillans,true))
	{
		if (OutVillans.Num() > 0)
		{
			
			for (AZeroLockCharacter* OutVillan : OutVillans)
			{
				if (OutVillan)
				{
					if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
					{
						EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
						return;
					}
					FVector StartLoc = Hero->GetActorLocation();

					FVector LaunchDir = (OutVillan->GetActorLocation() - StartLoc).GetUnsafeNormal2D();
					LaunchDir.Z = 0.75f;
					LaunchDir = LaunchDir.GetSafeNormal();

					
					float ImpulseDuration = 0.2f;    
					FVector LaunchVelocity = LaunchDir * GrappleLaunchStrength;
					FVector TargetLoc = StartLoc + (LaunchVelocity * ImpulseDuration);
					UZL_AbilityTask_MoverMoveTo* MovementTask = UZL_AbilityTask_MoverMoveTo::ApplyMoverMoveTo(this, FName("LaunchTask"), StartLoc, TargetLoc, ImpulseDuration,false);
					MovementTask->ReadyForActivation();
					if (OnGrappleEffect)
					{
						Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(Hero->GetMyAbilitySystemComp(),OnGrappleEffect,mylevel);
					}
					
					EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false);
					break;
				}
			}
			
		}
		else
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
		}
	}
	else
	{
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
	}
}
