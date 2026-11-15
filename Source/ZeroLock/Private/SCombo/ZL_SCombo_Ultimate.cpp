// Copyright Preetham Mukundan (C) 2026


#include "SCombo/ZL_SCombo_Ultimate.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "GAS/ZL_GE_MoveSlow.h"
#include "SCombo/ZL_SCombo_Combo.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_SCombo_Ultimate::UZL_SCombo_Ultimate()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_CHANNELING);
	bCancelOnStun = true;
}

bool UZL_SCombo_Ultimate::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || ZLSCombo::GetComboStacks(ActorInfo->AbilitySystemComponent.Get()) < MinComboToCast)
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UZL_SCombo_Ultimate::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Hero || !ASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bChannelActive = true;

	if (ChannelMontage)
	{
		ASC->PlayMontage(this, ActivationInfo, ChannelMontage, 1.0f);
	}

	// Jump only exists as mover input, so only the owning client sees it. Its EndAbility is replicated to the server.
	if (IsLocallyControlled())
	{
		JumpPressedHandle = Hero->OnJumpInputPressed.AddUObject(this, &UZL_SCombo_Ultimate::OnJumpPressed);
	}

	if (HasAuthority(&ActivationInfo))
	{
		StacksSpent = ZLSCombo::GetComboStacks(ASC);
		ASC->RemoveActiveGameplayEffectBySourceEffect(UZL_GE_SCombo_Combo::StaticClass(), nullptr, -1);

		// Not predicted: the multiplier is read where the input is made, and it replicates there as an attribute.
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UZL_GE_MoveSlow::StaticClass(), 1.0f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_MOVE_SPEED_MULTIPLIER, MoveSpeedMultiplier);
			Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_SLOWED);
			SlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}

		GetWorld()->GetTimerManager().SetTimer(DamageTimer, this, &UZL_SCombo_Ultimate::DamageTick, TickInterval, true, 0.0f);
	}

	ChannelTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, ChannelDuration.GetValueAtLevel(GetAbilityLevel()));
	ChannelTask->OnProgress.AddDynamic(this, &UZL_SCombo_Ultimate::UpdateProgressionTimer);
	ChannelTask->OnStarted.AddDynamic(this, &UZL_SCombo_Ultimate::StartProgressionTimer);
	ChannelTask->OnEnd.AddDynamic(this, &UZL_SCombo_Ultimate::StopProgressionTimer);
	ChannelTask->OnFinished.AddDynamic(this, &UZL_SCombo_Ultimate::OnChannelFinished);
	ChannelTask->ReadyForActivation();
}

void UZL_SCombo_Ultimate::DamageTick()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC)
	{
		return;
	}

	const int32 Level = GetAbilityLevel();
	const float DamageRadius = Radius.GetValueAtLevel(Level) * GetAbilityRangeScale();
	TArray<AZeroLockCharacter*> Villans;
	ZLSCombo::FindEnemiesInRadius(Hero, DamageRadius, Villans);

	const float Damage = TickDamage.GetValueAtLevel(Level) * (1.0f + DamagePerStack.GetValueAtLevel(Level) * StacksSpent);
	for (AZeroLockCharacter* Villan : Villans)
	{
		HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), Damage, this);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawUltimateDebug)
	{
		ZLDebugDraw::Sphere(GetWorld(), Hero->GetActorLocation(), DamageRadius, Villans.IsEmpty() ? FColor::Yellow : FColor::Red, TickInterval, 24);
	}
#endif
}

void UZL_SCombo_Ultimate::OnJumpPressed()
{
	// Jump only cancels the mode; swallow the press so it doesn't also jump.
	if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		Hero->ConsumeJumpPress();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UZL_SCombo_Ultimate::OnChannelFinished()
{
	// Each side has its own timer, and the client's end must not cut the server's last ticks short.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HasAuthority(&CurrentActivationInfo), false);
}

void UZL_SCombo_Ultimate::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bChannelActive)
	{
		bChannelActive = false;

		GetWorld()->GetTimerManager().ClearTimer(DamageTimer);

		if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (JumpPressedHandle.IsValid())
			{
				Hero->OnJumpInputPressed.Remove(JumpPressedHandle);
				JumpPressedHandle.Reset();
			}
		}

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (SlowHandle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(SlowHandle);
				SlowHandle.Invalidate();
			}
			if (ChannelMontage && ASC->GetCurrentMontage() == ChannelMontage)
			{
				ASC->CurrentMontageStop();
			}
		}

		if (ChannelTask && ChannelTask->IsActive())
		{
			ChannelTask->OnFinished.RemoveDynamic(this, &UZL_SCombo_Ultimate::OnChannelFinished);
			ChannelTask->EndTask();
		}
		ChannelTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
