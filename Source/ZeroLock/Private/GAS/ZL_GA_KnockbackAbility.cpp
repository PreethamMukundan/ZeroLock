// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_GA_KnockbackAbility.h"

#include "Mover/ZeroMoverComponent.h"
#include "Mover/ZeroMoverPawn.h"

void UZL_GA_KnockbackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                              const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                              const FGameplayEventData* TriggerEventData)
{
	//Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	float Duration = TriggerEventData ? TriggerEventData->EventMagnitude : 0.2f;
	FVector Velocity = FVector::ZeroVector;

	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		Velocity = TriggerEventData->TargetData.Get(0)->GetEndPoint();
	}

	if (AZeroMoverPawn* VictimPawn = Cast<AZeroMoverPawn>(GetAvatarActorFromActorInfo()))
	{
		if (UZeroMoverComponent* MoverComp = VictimPawn->FindComponentByClass<UZeroMoverComponent>())
		{
			// This safely sets bLatchedAbilityMove for the Mover's input generation
			MoverComp->RequestSafeAbilityMove(Velocity, Duration);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
