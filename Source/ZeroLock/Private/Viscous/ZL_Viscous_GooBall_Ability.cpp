// Copyright Preetham Mukundan (C) 2026


#include "Viscous/ZL_Viscous_GooBall_Ability.h"

#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "GAS/ZL_GameplayTags.h"
#include "Viscous/ZL_Viscous_GooBall.h"
#include "Viscous/ZL_Viscous_GooBallEffect.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Viscous_GooBall_Ability::UZL_Viscous_GooBall_Ability()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// Predicted on Viscous's machine: the Mover reads it into its input on the same frame.
	ActivationOwnedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_GOOBALL);
	ResistEffect = UZL_Viscous_GooBallEffect::StaticClass();
	BallClass = AZL_Viscous_GooBall::StaticClass();
	Slot = EGameplayAbilitySlot::UltimateSlot;
}

void UZL_Viscous_GooBall_Ability::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                                  const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bBallEnded = false;
	ResistHandle.Invalidate();
	BallActor.Reset();

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	// The cooldown starts when the ball ends (EndAbility commits); refuse now if it's still on cooldown.
	if (!Hero || !CheckCooldown(Handle, ActorInfo))
	{
		// Never started: nothing to clean up, no cooldown.
		bBallEnded = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 Level = GetAbilityLevel();
	if (HasAuthority(&ActivationInfo))
	{
		UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
		if (HeroASC && ResistEffect)
		{
			FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(ResistEffect, 1.0f, HeroASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_BULLET_RESIST, BulletResist.GetValueAtLevel(Level));
				Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_SPIRIT_RESIST, SpiritResist.GetValueAtLevel(Level));
				ResistHandle = HeroASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}

		if (BallClass)
		{
			FActorSpawnParameters Params;
			Params.Owner = Hero;
			Params.Instigator = Hero;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AZL_Viscous_GooBall* Ball = GetWorld()->SpawnActor<AZL_Viscous_GooBall>(BallClass, Hero->GetActorTransform(), Params))
			{
				Ball->StartBall(Hero, ImpactDamage.GetValueAtLevel(Level), StunEffect, StunDuration.GetValueAtLevel(Level));
				BallActor = Ball;
			}
		}
	}

	// HUD bar for how long the ball lasts.
	DurationTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, Duration.GetValueAtLevel(Level));
	DurationTask->OnProgress.AddDynamic(this, &UZL_Viscous_GooBall_Ability::UpdateProgressionTimer);
	DurationTask->OnStarted.AddDynamic(this, &UZL_Viscous_GooBall_Ability::StartProgressionTimer);
	DurationTask->OnEnd.AddDynamic(this, &UZL_Viscous_GooBall_Ability::StopProgressionTimer);
	DurationTask->OnFinished.AddDynamic(this, &UZL_Viscous_GooBall_Ability::OnDurationFinished);
	DurationTask->ReadyForActivation();

	RepressTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	RepressTask->OnPress.AddDynamic(this, &UZL_Viscous_GooBall_Ability::OnRepressed);
	RepressTask->ReadyForActivation();
}

void UZL_Viscous_GooBall_Ability::OnDurationFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_Viscous_GooBall_Ability::OnRepressed(float TimeWaited)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_Viscous_GooBall_Ability::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                             const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bBallEnded)
	{
		bBallEnded = true;

		if (RepressTask)
		{
			RepressTask->OnPress.RemoveAll(this);
			RepressTask->EndTask();
			RepressTask = nullptr;
		}
		if (DurationTask)
		{
			DurationTask->OnFinished.RemoveAll(this);
			DurationTask->EndTask();
			DurationTask = nullptr;
		}

		if (AZL_Viscous_GooBall* Ball = BallActor.Get())
		{
			Ball->Destroy();
		}
		BallActor.Reset();

		if (ResistHandle.IsValid())
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->RemoveActiveGameplayEffect(ResistHandle);
			}
			ResistHandle.Invalidate();
		}

		// Cooldown from the end of the ball.
		CommitAbility(Handle, ActorInfo, ActivationInfo);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
