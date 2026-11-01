// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GA_AimZoom.h"

#include "Mover/ZeroMoverPawn.h"

UZL_GA_AimZoom::UZL_GA_AimZoom()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	// Never commits. Without this the BP compiler flags UZL_GE_BaseCooldown ("grants no tags") on every subclass.
	CooldownGameplayEffectClass = nullptr;
}

void UZL_GA_AimZoom::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroMoverPawn* Hero = Cast<AZeroMoverPawn>(GetAvatarActorFromActorInfo());
	if (!Hero || !Hero->IsLocallyControlled())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	Hero->SetAimZoom(true, ZoomFOVRelative, ZoomBlendTime);

	// Released in the same frame it was pressed.
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (Spec && !Spec->InputPressed)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	}
}

void UZL_GA_AimZoom::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                   const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

void UZL_GA_AimZoom::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AZeroMoverPawn* Hero = Cast<AZeroMoverPawn>(GetAvatarActorFromActorInfo()))
	{
		Hero->SetAimZoom(false, ZoomFOVRelative, ZoomBlendTime);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
