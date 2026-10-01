// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GA_PullToAbility.h"

#include "Mover/ZeroMoverComponent.h"
#include "Mover/ZeroMoverPawn.h"

void UZL_GA_PullToAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                           const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                           const FGameplayEventData* TriggerEventData)
{
	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		const FVector Target = TriggerEventData->TargetData.Get(0)->GetEndPoint();
		const float Duration = TriggerEventData->EventMagnitude;

		if (AZeroMoverPawn* Pawn = Cast<AZeroMoverPawn>(GetAvatarActorFromActorInfo()))
		{
			if (UZeroMoverComponent* Mover = Pawn->FindComponentByClass<UZeroMoverComponent>())
			{
				Mover->RequestSafePullTo(Target, Duration);
			}
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
