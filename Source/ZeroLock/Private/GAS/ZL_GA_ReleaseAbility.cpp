// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GA_ReleaseAbility.h"

#include "Mover/ZeroMoverComponent.h"
#include "Mover/ZeroMoverPawn.h"

void UZL_GA_ReleaseAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		if (AZeroMoverPawn* Pawn = Cast<AZeroMoverPawn>(GetAvatarActorFromActorInfo()))
		{
			if (UZeroMoverComponent* Mover = Pawn->FindComponentByClass<UZeroMoverComponent>())
			{
				Mover->RequestSafeRelease();
			}
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
