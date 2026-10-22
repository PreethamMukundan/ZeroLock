//Copyright Preetham Mukundan (C) 2026


#include "Drifter/ZL_Drifter_Rend.h"

#include "Engine/OverlapResult.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Drifter_Rend::UZL_Drifter_Rend()
{
	ConeAngle=45.0f;
	ConeRadius =100;
	BonusTargetDist =50;
	BaseDamage =25;
	
}

void UZL_Drifter_Rend::OnAnimationPointTrigger()
{

		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
		if (!Hero)
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
		}
	
		TArray<FOverlapResult> OverlapResults;
		FVector Origin = Hero->GetActorLocation();
		FVector Direction = Hero->GetActorForwardVector();
		TArray<FHitResult> HitResults;
		TArray<AActor*> ActorsToIgnore;
		TArray<AZeroLockCharacter*> OutVillans;
		ActorsToIgnore.Add(Hero);
		CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
		if (ConeTraceMulti(GetWorld(),Origin,Direction.Rotation(),ConeHeight,ConeAngle/2, UEngineTypes::ConvertToTraceType(ECC_Pawn),true,ActorsToIgnore,EDrawDebugTrace::ForDuration,HitResults,OutVillans,true))
		{
			if (OutVillans.Num() > 0)
			{
				for (AZeroLockCharacter* villan : OutVillans)
				{
					ZLOG("One");
					Hero->GetMyAbilitySystemComp()->ApplyMeleeDamage(villan->GetMyAbilitySystemComp(),BaseDamage, this);
					float DistanceSq = FVector::DistSquared(Origin, villan->GetActorLocation());
					if (DistanceSq <= (BonusTargetDist*BonusTargetDist))
					{
						Hero->GetMyAbilitySystemComp()->ApplySpiritDamage(villan->GetMyAbilitySystemComp(),BonusDamage.GetValueAtLevel(GetCurrentAbilitySpec()->Level), this);
					}
				}
			}
	
		}
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false);
	

	
}
