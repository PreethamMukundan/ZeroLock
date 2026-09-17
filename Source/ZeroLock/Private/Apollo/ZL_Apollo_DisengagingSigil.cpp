// Copyright Preetham Mukundan (C) 2026


#include "Apollo/ZL_Apollo_DisengagingSigil.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_AbilityTask_MoverMoveTo.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Apollo_DisengagingSigil::UZL_Apollo_DisengagingSigil()
{
}

void UZL_Apollo_DisengagingSigil::OnMoveComplete()
{
   EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Apollo_DisengagingSigil::OnAnimationPointTrigger()
{
   AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
    if (!Hero)
    {
       EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
       return; 
    }
    
    FVector Start = Hero->GetActorLocation();
    FVector End = Start + (Hero->GetFollowCamera()->GetForwardVector() * 200.0f);
    FQuat Rotation = Hero->GetActorQuat();
    FCollisionShape Capsule = FCollisionShape::MakeCapsule(EffectRadius, 100.f);
    
   
   if (UAbilitySystemComponent* ASC = GetCurrentActorInfo()->AbilitySystemComponent.Get())
   {
      ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
   }
    DrawDebugCapsule(GetWorld(), End, 50, EffectRadius, Rotation, FColor::Red, false, 10, 1);
   
    if (Hero->HasAuthority())
    {
        TArray<FHitResult> HitResults;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(Hero);
        
        bool bHit = GetWorld()->SweepMultiByChannel(HitResults, Start, End, Rotation, ECC_Pawn, Capsule, Params);

        if (bHit && HitResults.Num() > 0)
        {
           for (int i = 0; i < HitResults.Num(); i++)
           {
              if (AZeroLockCharacter* HitHero = Cast<AZeroLockCharacter>(HitResults[i].GetActor()))
              {
                 if(!Hero->IsOnSameTeam(HitHero))
                 {
                    Hero->GetMyAbilitySystemComp()->ApplySpiritDamage(HitHero->GetMyAbilitySystemComp(), BaseDamageValue.GetValueAtLevel(GetAbilityLevel()));
                 }
              }
           }
        }
    }


    FVector ForwardCM = Hero->GetActorForwardVector().GetSafeNormal();
    ForwardCM.Z = 0;
    FVector LaunchVelocity = ForwardCM * (-1 * KnockbackStrength);
   
   FVector MoveDir = Hero->GetInputWorldDir();
   FVector moveLaunch = LaunchVelocity + (MoveDir * KnockbackStrength);
   moveLaunch.Z = 0;
   FVector StartLoc = Hero->GetActorLocation();
   FVector TargetLoc = StartLoc + (moveLaunch) + FVector(0, 0, 100);
 
	DrawDebugDirectionalArrow(GetWorld(), StartLoc, TargetLoc,20,FColor::Red, false, 10, 1);
   DrawDebugSphere(GetWorld(),StartLoc,100,30,FColor::Yellow);
   UZL_AbilityTask_MoverMoveTo* MovementTask = UZL_AbilityTask_MoverMoveTo::ApplyMoverMoveTo(this, FName("LaunchTask"), StartLoc, TargetLoc, 0.1,false);
   MovementTask->OnFinished.AddDynamic(this, &UZL_Apollo_DisengagingSigil::OnMoveComplete);
   MovementTask->ReadyForActivation();
   // Hero->LaunchCharacter(LaunchVelocity, true, true);


    if (KnockBackMontage)
    {
       Hero->GetAbilitySystemComponent()->PlayMontage(this, GetCurrentActivationInfo(), KnockBackMontage, 1);
    }
   
}
