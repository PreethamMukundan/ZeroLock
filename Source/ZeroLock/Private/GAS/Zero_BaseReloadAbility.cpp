//Copyright Preetham Mukundan (C) 2026


#include "GAS/Zero_BaseReloadAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Animation/ZL_DeadlockAnimSet.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/ZL_GameplayTags.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZero_BaseReloadAbility::UZero_BaseReloadAbility()
{
	// Blocks UZL_GA_WeaponFire for the whole reload.
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_WEAPON_RELOADING);
}

bool UZero_BaseReloadAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// Nothing to reload with a full clip.
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		bool bFoundCurrent = false, bFoundMax = false;
		const float Current = ASC->GetGameplayAttributeValue(UBaseCharAttributeSet::GetCurrentAmmoAttribute(), bFoundCurrent);
		const float Max = ASC->GetGameplayAttributeValue(UBaseCharAttributeSet::GetMaxAmmoAttribute(), bFoundMax);
		if (bFoundCurrent && bFoundMax && Current >= Max)
		{
			return false;
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

float UZero_BaseReloadAbility::GetReloadDuration() const
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	return FMath::Max(0.05f, Hero ? Hero->GetAttributeOr(UBaseCharAttributeSet::GetReloadTimeAttribute(), ReloadTime) : ReloadTime);
}

UAnimMontage* UZero_BaseReloadAbility::FindReloadMontage() const
{
	if (ReloadMontage)
	{
		return ReloadMontage;
	}
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	return Hero && Hero->DeadlockAnimSet ? Hero->DeadlockAnimSet->ReloadMontage.Get() : nullptr;
}

void UZero_BaseReloadAbility::OnFinish()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* AbilitySystemComp = Hero ? Hero->GetAbilitySystemComponent() : nullptr;
	// The refill is server-only; the client gets the ammo by replication (it can't apply an instant GE outside a
	// prediction window anyway).
	if (AbilitySystemComp && ReloadEffect && HasAuthority(&CurrentActivationInfo))
	{
		FGameplayEffectContextHandle EffectContext =AbilitySystemComp->MakeEffectContext();
		EffectContext.AddSourceObject(this);


		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComp->MakeOutgoingSpec(ReloadEffect, 1, EffectContext);

		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle GEHandle = AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
	// Only the server replicates its end: the client's timer can finish first, and its replicated end used to cancel
	// the server's reload before the refill.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HasAuthority(&CurrentActivationInfo), false);
}

void UZero_BaseReloadAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                              const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                              const FGameplayEventData* TriggerEventData)
{
	const float Duration = GetReloadDuration();

	// Stretch the clip to the reload time. The timer below, not the montage, ends the ability.
	if (UAnimMontage* Montage = FindReloadMontage())
	{
		const float Rate = Montage->GetPlayLength() > 0.f ? Montage->GetPlayLength() / Duration : 1.f;
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, Rate);
		MontageTask->ReadyForActivation();
	}

	TaskWaitDelay = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	TaskWaitDelay->OnFinish.AddDynamic(this,&UZero_BaseReloadAbility::OnFinish);
	TaskWaitDelay->ReadyForActivation();
}

void UZero_BaseReloadAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
