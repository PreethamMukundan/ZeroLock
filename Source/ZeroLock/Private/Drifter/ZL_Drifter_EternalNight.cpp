//Copyright Preetham Mukundan (C) 2026


#include "Drifter/ZL_Drifter_EternalNight.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Zero_BasePlayerState.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/OverlapResult.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"


void UZL_Drifter_EternalNight::OnAnimationPointTrigger()
{
	if (!GetCurrentActorInfo()->IsNetAuthority()) return;
	AZeroLockCharacter* Avatar = Cast<AZeroLockCharacter>(GetCurrentActorInfo()->AvatarActor.Get());
	if (!Avatar)
	{
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(), true, true);
	}
	FVector Origin = Avatar->GetActorLocation();
	if (PlayerBuffEffectClass)
	{
		if (!PlayerBuffEffectHandle.IsValid())
		{
		
			ZLOG("Effect Applied");
			FGameplayEffectContextHandle EffectContext = Avatar->GetAbilitySystemComponent()->MakeEffectContext();
			EffectContext.AddSourceObject(this);

			FGameplayEffectSpecHandle SpecHandle = Avatar->GetAbilitySystemComponent()->MakeOutgoingSpec(PlayerBuffEffectClass, GetAbilityLevel(), EffectContext);
            
			if (SpecHandle.IsValid())
			{
				PlayerBuffEffectHandle = Avatar->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(MaxRange);
	GetWorld()->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, Sphere);
	TArray<AZeroLockCharacter*> EnemyToEffect;
	for (FOverlapResult& OverlapResult : Overlaps)
	{
		if (AZeroLockCharacter* OverlapActor = Cast<AZeroLockCharacter>(OverlapResult.GetActor()))
		{
			AZero_BasePlayerState* Target_PS = OverlapActor->GetPlayerState<AZero_BasePlayerState>();
			AZero_BasePlayerState* Source_PS = Avatar->GetPlayerState<AZero_BasePlayerState>();
			if (Source_PS && Target_PS)
			{
				if (Source_PS->TeamID != Target_PS->TeamID)
				{
					EnemyToEffect.AddUnique(OverlapActor);
				}
			}
		}
	}
	CurrentActiveEffectHandles.Empty();
	if (BlindEffectClass)
	{
		FGameplayEffectContextHandle Context = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
		FGameplayEffectSpecHandle Spec = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(BlindEffectClass, 1.f, Context);
			
		int32 MaxTargetsInt =FMath::FloorToInt32(MaxTargets.GetValueAtLevel(GetAbilityLevel()));
		for (int32 i = 0; i < FMath::Min(MaxTargetsInt, EnemyToEffect.Num()); ++i)
		{
				UAbilitySystemComponent* TargetASC = EnemyToEffect[i]->GetAbilitySystemComponent();
				if (TargetASC)
				{
					FActiveGameplayEffectHandle ActiveHandle = Avatar->GetMyAbilitySystemComp()->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(),TargetASC);
					if (ActiveHandle.IsValid())
					{
						CurrentActiveEffectHandles.Add(ActiveHandle);
					}
				}
			
		}
	}
	CommitAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo());
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration.GetValueAtLevel(GetAbilityLevel()));
	DelayTask->OnFinish.AddDynamic(this, &UZL_Drifter_EternalNight::OnDelayFinished);
	DelayTask->ReadyForActivation();
	UAbilityTask_WaitGameplayEvent* WaitGameplayEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,FGameplayTag::RequestGameplayTag("Event.WeaponHit"));
	WaitGameplayEventTask->EventReceived.AddDynamic(this,&UZL_Drifter_EternalNight::OnWeaponEventTrigger);
	WaitGameplayEventTask->ReadyForActivation();
}

void UZL_Drifter_EternalNight::OnAnimationCompleted()
{
	//Super::OnAnimationCompleted();
	if (UAbilitySystemComponent* ASC = GetCurrentActorInfo()->AbilitySystemComponent.Get())
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("ZeroLock.Abilities.MovementLock")));
	}
}


void UZL_Drifter_EternalNight::OnWeaponEventTrigger(FGameplayEventData Payload)
{
	const AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Payload.Target);
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Villan)) return;
	if (!IsValid(Hero)) return;
	Hero->GetMyAbilitySystemComp()->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(),SpiritDamage.GetValueAtLevel(GetAbilityLevel()), this);
}



void UZL_Drifter_EternalNight::OnDelayFinished()
{
	for (FActiveGameplayEffectHandle& Handle : CurrentActiveEffectHandles)
	{
		if (Handle.IsValid())
		{
			UAbilitySystemComponent* TargetASC = Handle.GetOwningAbilitySystemComponent();
			if (TargetASC)
			{
				TargetASC->RemoveActiveGameplayEffect(Handle);
			}
		}
	}
	
	if (PlayerBuffEffectHandle.IsValid())
	{
		if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
		{
			
			Hero->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(PlayerBuffEffectHandle);
			PlayerBuffEffectHandle.Invalidate();
		}
		
	}
	CurrentActiveEffectHandles.Empty();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
