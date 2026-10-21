//Copyright Preetham Mukundan (C) 2026


#include "GAS/Base_GA_TargetActors.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
//#include "GAS/Tasks/GAST_WaitTargetData.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "ZeroLock/ZeroLock.h"
UBase_GA_TargetActors::UBase_GA_TargetActors()
{
	
}

void UBase_GA_TargetActors::TargetConfirmed(const FGameplayAbilityTargetDataHandle& Data)
{
	ZLOG("TargetConfirmed");
	CommitAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo);
	AbilityConfirmedAction(Data);
}

void UBase_GA_TargetActors::TargetCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	ZLOG("TargetCancelled");
	AbilityCancelledAction(Data);
}

void UBase_GA_TargetActors::TargetInputRelease(float TimeHeld)
{
	
	if (WaitTargetTask->IsActive())
	{
		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
		if (Hero)
		{
			
			Hero->GetAbilitySystemComponent()->TargetConfirm();
		}
	}
}

void UBase_GA_TargetActors::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	
	FVector StartLocation = GetAvatarActorFromActorInfo()->GetActorLocation();
	FTransform SpawnTransform = FTransform::Identity;
	SpawnTransform.SetLocation(StartLocation);

	AGameplayAbilityTargetActor_Trace* TargetActor = GetWorld()->SpawnActor<AGameplayAbilityTargetActor_Trace>(Targetclass,SpawnTransform);

	TargetActor->OwningAbility = this;
	TargetActor->PrimaryPC = Cast<APlayerController>(ActorInfo->PlayerController.Get());
	TargetActor->SetOwner(GetAvatarActorFromActorInfo());

	
	TargetActor->StartLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	TargetActor->StartLocation.LiteralTransform = SpawnTransform;

	FGameplayTargetDataFilter* Filter = new FGameplayTargetDataFilter();
	Filter->RequiredActorClass = AZeroLockCharacter::StaticClass();
	Filter->SelfFilter = ETargetDataFilterSelf::TDFS_NoSelf;

	TSharedPtr<FGameplayTargetDataFilter> TargetFilter(Filter);
	//Filter->SelfActor = GetAvatarActorFromActorInfo();

	FGameplayTargetDataFilterHandle FilterHandle(TargetFilter);
	
	TargetActor->Filter = FilterHandle;

	OnTargetActorSpawned(TargetActor);

	TargetActor->StartTargeting(this);

	TargetActor->ReticleClass = ReticleClassX;
	if (TargetConfirmationStyle == EGASTargetConfirmationStyle::Instant)
	{
		WaitTargetTask = UAbilityTask_WaitTargetData::WaitTargetDataUsingActor(this,FName(TEXT("Targeting")),	EGameplayTargetingConfirmation::Instant,TargetActor);
	}
	else
	{
		WaitTargetTask = UAbilityTask_WaitTargetData::WaitTargetDataUsingActor(this,FName(TEXT("Targeting")),	EGameplayTargetingConfirmation::UserConfirmed,TargetActor);
	}
	WaitTargetTask->ValidData.AddDynamic(this, &UBase_GA_TargetActors::TargetConfirmed);
	WaitTargetTask->Cancelled.AddDynamic(this, &UBase_GA_TargetActors::TargetCancelled);

	WaitTargetTask->ReadyForActivation();

	if (TargetConfirmationStyle == EGASTargetConfirmationStyle::Quick)
	{
		UAbilityTask_WaitInputRelease* InputRTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this,true);
		InputRTask->OnRelease.AddDynamic(this, &UBase_GA_TargetActors::TargetInputRelease);
		InputRTask->ReadyForActivation();
	}
	
}

void UBase_GA_TargetActors::AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data)
{
	ZLOG("Confirmed");
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBase_GA_TargetActors::AbilityCancelledAction(const FGameplayAbilityTargetDataHandle& Data)
{
	ZLOG("Cancelled");
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);

}


