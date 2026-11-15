// Copyright Preetham Mukundan (C) 2026


#include "SCombo/ZL_SCombo_QuickAoE.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Debug/ZL_DebugDraw.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "SCombo/ZL_SCombo_Combo.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_SCombo_QuickAoE::UZL_SCombo_QuickAoE()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_UNTOUCHABLE);
}

void UZL_SCombo_QuickAoE::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && AoEMontage)
	{
		ASC->PlayMontage(this, ActivationInfo, AoEMontage, 1.0f);
	}

	UAbilityTask_WaitDelay* AoETask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Min(AoEDelay, UntouchableTime));
	AoETask->OnFinish.AddDynamic(this, &UZL_SCombo_QuickAoE::OnAoEDelayFinished);
	AoETask->ReadyForActivation();

	UAbilityTask_WaitDelay* EndTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(UntouchableTime, 0.01f));
	EndTask->OnFinish.AddDynamic(this, &UZL_SCombo_QuickAoE::OnUntouchableFinished);
	EndTask->ReadyForActivation();
}

void UZL_SCombo_QuickAoE::OnAoEDelayFinished()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	const int32 Level = GetAbilityLevel();
	const float Radius = AoERadius.GetValueAtLevel(Level) * GetAbilityRangeScale();
	TArray<AZeroLockCharacter*> Villans;
	ZLSCombo::FindEnemiesInRadius(Hero, Radius, Villans);

	const float Damage = AoEDamage.GetValueAtLevel(Level);
	for (AZeroLockCharacter* Villan : Villans)
	{
		HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), Damage, this);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawAoEDebug)
	{
		ZLDebugDraw::Sphere(GetWorld(), Hero->GetActorLocation(), Radius, Villans.IsEmpty() ? FColor::Yellow : FColor::Red, 1.0f, 24);
	}
#endif
}

void UZL_SCombo_QuickAoE::OnUntouchableFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_SCombo_QuickAoE::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The client's timers run first, and replicating that end could cancel the server's instance before the damage.
	// Real cancels still reach the server: CancelAbility replicates on its own.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
