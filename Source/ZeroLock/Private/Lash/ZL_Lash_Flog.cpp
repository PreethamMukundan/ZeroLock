// Copyright Preetham Mukundan (C) 2026


#include "Lash/ZL_Lash_Flog.h"

#include "Camera/CameraComponent.h"
#include "Engine/OverlapResult.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Lash_Flog::UZL_Lash_Flog()
{
}

void UZL_Lash_Flog::OnAnimationPointTrigger()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero)
	{
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
		return;
	}
	CommitAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo());
	TArray<FOverlapResult> OverlapResults;
	FVector Origin = Hero->GetPawnViewLocation();
	FVector Direction = GetHeroAimDirection();
	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore;
	TArray<AZeroLockCharacter*> OutVillans;
	ActorsToIgnore.Add(Hero);
	int32 myLevel = GetCurrentAbilitySpec()->Level;
	//CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	if (ConeTraceMulti(GetWorld(),Origin,Direction.Rotation(),ConeHeight,ConeAngle/2, UEngineTypes::ConvertToTraceType(ECC_Pawn),true,ActorsToIgnore,EDrawDebugTrace::ForDuration,HitResults,OutVillans,true))
	{
		if (GetCurrentActorInfo()->IsNetAuthority())
		{
			if (OutVillans.Num() > 0)
			{
				for (AZeroLockCharacter* villan : OutVillans)
				{
					if (!villan) continue;
					if (Hero->IsOnSameTeam(villan)) continue;

					if (FlogEffect)
					{
						Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(villan->GetMyAbilitySystemComp(),FlogEffect,myLevel);
					}
					Hero->GetMyAbilitySystemComp()->ApplySpiritDamage(villan->GetMyAbilitySystemComp(),FlogDamage.GetValueAtLevel(myLevel), this);
					Hero->GetMyAbilitySystemComp()->ApplyHeal(Hero->GetMyAbilitySystemComp(),(FlogDamage.GetValueAtLevel(myLevel)*0.6));
					
				}
			}
		}
		
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_Lash_Flog::OnAnimationCompleted()
{
	//Super::OnAnimationCompleted();
	OnAnimationPointTrigger();
}
