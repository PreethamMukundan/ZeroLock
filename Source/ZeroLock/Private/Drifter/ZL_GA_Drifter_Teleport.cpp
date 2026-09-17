//Copyright Preetham Mukundan (C) 2026


#include "Drifter/ZL_GA_Drifter_Teleport.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"




void UZL_GA_Drifter_Teleport::OnEventRecived(FGameplayEventData Payload)
{
	//Super::OnEventRecived(Payload);
	Villan = Cast<AZeroLockCharacter>(Payload.Target);
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Villan)) return;
	if (!IsValid(Hero)) return;

	UAbilitySystemComponent* TargetASC = Villan->GetAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = Hero->GetAbilitySystemComponent();

	if (!TargetASC || !SourceASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	
	if (MarkEffectClass)
	{
		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddInstigator(Hero, Hero);

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(MarkEffectClass, GetAbilityLevel(), EffectContext);
		if (SpecHandle.IsValid())
		{
			AppliedEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}
	}
	ZLOG("EnemyHitRecieved");
	
	TargetASC->AddGameplayCue(MarkedCueTag);
	InputPTask = UAbilityTask_WaitInputPress::WaitInputPress(this,false);
	InputPTask->OnPress.AddDynamic(this, &ThisClass::OnInputPressed);
	InputPTask->ReadyForActivation();

	WaitDelay = UAbilityTask_WaitDelay::WaitDelay(this,DurationOfMark.GetValueAtLevel(GetAbilityLevel()));
	WaitDelay->OnFinish.AddDynamic(this, &ThisClass::OnTimeFinish);
	WaitDelay->ReadyForActivation();
	
}

void UZL_GA_Drifter_Teleport::OnInputPressed(float TimeWaited)
{
	WaitDelay->ExternalCancel();
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (AfterTeleportEffectClass)
	{
		Hero->GetMyAbilitySystemComp()->ApplyGameplayEffect(Hero->GetMyAbilitySystemComp(),AfterTeleportEffectClass,GetCurrentAbilitySpec()->Level);
	}
	if (Villan && AppliedEffectHandle.IsValid())
	{
		Villan->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(AppliedEffectHandle);
	}
	if (Villan && IsValid(Hero))
	{
		FVector ForwardDirection = Villan->GetActorForwardVector().GetSafeNormal();
		
		Hero->SetActorLocation(Villan->GetActorLocation()+ ForwardDirection* -125);
		Hero->GetController()->SetControlRotation(Villan->GetControlRotation());
		//Hero->GetCharacterMovement()->StopMovementImmediately();
		//Hero->SetActorRotation(Villan->GetActorRotation(),ETeleportType::TeleportPhysics);
		Villan->GetAbilitySystemComponent()->RemoveGameplayCue(MarkedCueTag);
	}
	//CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

}

void UZL_GA_Drifter_Teleport::OnTimeFinish()
{
	if (Villan && AppliedEffectHandle.IsValid())
	{
		Villan->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(AppliedEffectHandle);
	}
	if (Villan && IsValid(Villan))
	{
		Villan->GetAbilitySystemComponent()->RemoveGameplayCue(MarkedCueTag);
	}
	//CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_GA_Drifter_Teleport::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{

	if (Villan && AppliedEffectHandle.IsValid())
	{
		Villan->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(AppliedEffectHandle);
		Villan->GetAbilitySystemComponent()->RemoveGameplayCue(MarkedCueTag);
	}
	CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
