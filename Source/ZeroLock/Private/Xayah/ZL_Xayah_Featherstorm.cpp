// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_Featherstorm.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/RepLayout.h"
#include "Xayah/ZL_Xayah_projectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Xayah_Featherstorm::UZL_Xayah_Featherstorm()
{
}

void UZL_Xayah_Featherstorm::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero)
	{
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
	}

	FVector BackwardsDir = -Hero->GetActorForwardVector();

	FVector LaunchVel = FVector::ZeroVector;
	LaunchVel.Z = 1;
	LaunchVel *= LaunchStrength;
	
	//Hero->LaunchCharacter(LaunchVel, true, true);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FireDelay);
	DelayTask->OnFinish.AddDynamic(this, &UZL_Xayah_Featherstorm::FireFeathers);
	DelayTask->ReadyForActivation();
}

void UZL_Xayah_Featherstorm::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero)
	{
		//Hero->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZL_Xayah_Featherstorm::FireFeathers()
{
	if (UAbilitySystemComponent* ASC = GetCurrentActorInfo()->AbilitySystemComponent.Get())
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	//Hero->GetCharacterMovement()->BrakingDecelerationFlying =1000;
	//Hero->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	
	AGameplayAbilityTargetActor_GroundTrace* MyTargetActor = GetWorld()->SpawnActor<AGameplayAbilityTargetActor_GroundTrace>(TargetActorClass);

	if (MyTargetActor)
	{
		FGameplayAbilityTargetingLocationInfo StartLocationInfo;
		StartLocationInfo.LocationType = EGameplayAbilityTargetingLocationType::ActorTransform;
		StartLocationInfo.SourceActor = GetAvatarActorFromActorInfo();
		StartLocationInfo.SourceAbility = this;

		MyTargetActor->bTraceAffectsAimPitch = true;
		MyTargetActor->TraceProfile.Name = TEXT("WorldStatic");
		MyTargetActor->StartLocation = StartLocationInfo;
		MyTargetActor->CollisionRadius = 20.0f;
		MyTargetActor->MaxRange = 200.0f;
		MyTargetActor->bDestroyOnConfirmation = true;
		MyTargetActor->bDebug = true;
		MyTargetActor->OwningAbility = this;
		MyTargetActor->PrimaryPC = Cast<APlayerController>(GetCurrentActorInfo()->PlayerController.Get());
		MyTargetActor->SetOwner(GetAvatarActorFromActorInfo());
	}
	
	UAbilityTask_WaitTargetData* WaitTask = UAbilityTask_WaitTargetData::WaitTargetDataUsingActor(this,TEXT("WaitTargetData"),EGameplayTargetingConfirmation::UserConfirmed,MyTargetActor);

	if (!WaitTask)
	{
		ZLOG("Error: WaitTargetData is NULL");
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}
	WaitTask->ValidData.AddDynamic(this, &UZL_Xayah_Featherstorm::OnTargetDataReady);
	WaitTask->ReadyForActivation();
	
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, TargetingDelay);
	DelayTask->OnFinish.AddDynamic(this, &UZL_Xayah_Featherstorm::TargetTimeDone);
	DelayTask->ReadyForActivation();
	
}


void UZL_Xayah_Featherstorm::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data)
{
	if (HasAuthority(&CurrentActivationInfo))
    {
        AActor* AvatarActor = GetAvatarActorFromActorInfo();
        UWorld* World = GetWorld();
        if (!AvatarActor || !World) return;


        FVector ActorLoc = AvatarActor->GetActorLocation();
        FVector Forward = AvatarActor->GetActorForwardVector();


        FVector TargetLoc = ActorLoc + (Forward * 500.0f); 
        if (Data.Num() > 0 && Data.Get(0)->GetHitResult())
        {
            TargetLoc = Data.Get(0)->GetHitResult()->Location;
        }
        TargetLoc += (Forward * 400.0f);

        FVector MuzzleLoc = ActorLoc + FVector(0, 0, 50.f); 
        FVector AimDir = (TargetLoc - MuzzleLoc).GetSafeNormal();

        float AngleStep = (ProjectileCount > 1) ? (ConeAngle / (ProjectileCount - 1)) : 0.0f;
        float CurrentAngle = -ConeAngle / 2.0f;

        for (int32 i = 0; i < ProjectileCount; i++)
        {

            FVector ProjDir = AimDir.RotateAngleAxis(CurrentAngle, FVector::UpVector);        
            
            FTransform SpawnTransform(ProjDir.Rotation(), MuzzleLoc + (ProjDir * 20.0f));

            AZL_Xayah_projectile* Proj = World->SpawnActorDeferred<AZL_Xayah_projectile>(
                ProjectileClass, SpawnTransform, AvatarActor, Cast<APawn>(AvatarActor));
            
            if (Proj)
            {
                Proj->SetAutoPull(true);
                Proj->FinishSpawning(SpawnTransform);
            }
            CurrentAngle += AngleStep;
        }

        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UZL_Xayah_Featherstorm::TargetTimeDone()
{
	if (UAbilitySystemComponent* ASC = GetCurrentActorInfo()->AbilitySystemComponent.Get())
	{
		ASC->LocalInputConfirm();
	}
}
