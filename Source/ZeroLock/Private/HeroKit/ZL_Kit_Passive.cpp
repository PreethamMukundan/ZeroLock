// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_Passive.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Items/ZL_ItemProcComponent.h"
#include "Items/Zero_Item_data.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Kit_Passive::UZL_Kit_Passive()
{
	TargetStyle = EGASTargetConfirmationStyle::Passive;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

AZeroLockCharacter* UZL_Kit_Passive::GetHero() const
{
	return Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
}

void UZL_Kit_Passive::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
}

void UZL_Kit_Passive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEnding = false;
	AZeroLockCharacter* Hero = GetHero();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Hero || !ASC || !HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!Procs.IsEmpty() && Hero->GetItemProcs())
	{
		// The proc component runs item data: the passive hands it a transient item of its own.
		ProcItem = NewObject<UZero_Item_data>(this);
		ProcItem->ItemID = GetClass()->GetFName();
		ProcItem->Procs = Procs;
		Hero->GetItemProcs()->AddItem(ProcItem);
	}

	if (!PermanentStats.IsEmpty())
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UZL_GE_ItemStats::StaticClass(), 1.0f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, PermanentStats);
			StatsHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
	ASC->AddLooseGameplayTags(PermanentTags);
	bTagsAdded = true;

	UAbilityTask_WaitGameplayEvent* KillTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(TEXT("Event.Kill")), nullptr, false);
	KillTask->EventReceived.AddDynamic(this, &UZL_Kit_Passive::HandleKill);
	KillTask->ReadyForActivation();

	if (!OnHitPayload.IsEmpty() || OnHitPayload.HitCue.IsSet())
	{
		if (UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp())
		{
			DamageDealtHandle = HeroASC->OnDamageDealt.AddUObject(this, &UZL_Kit_Passive::HandleDamageDealt);
		}
	}
	NextOnHitTime = 0.0;

	OnPassiveStarted();
}

void UZL_Kit_Passive::HandleDamageDealt(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource)
{
	AZeroLockCharacter* Hero = GetHero();
	if ((OnHitPayload.IsEmpty() && !OnHitPayload.HitCue.IsSet()) || DamageSource == this || !Hero || !IsValid(Target) || Target->bIsDead)
	{
		return;
	}
	const bool bCounts = OnHitTriggers.IsEmpty()
		? (Trigger == EZL_ProcTrigger::BulletHit || Trigger == EZL_ProcTrigger::MeleeHit || Trigger == EZL_ProcTrigger::HeavyMeleeHit)
		: OnHitTriggers.Contains(Trigger);
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bCounts || Now < NextOnHitTime || (OnHitChance > 0.0f && FMath::FRand() * 100.0f >= OnHitChance))
	{
		return;
	}
	NextOnHitTime = Now + OnHitCooldown;
	ZLKit::ApplyPayload(this, Hero, Target, Hero->GetActorLocation(), OnHitPayload);
}

void UZL_Kit_Passive::HandleKill(FGameplayEventData Payload)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero || Hero->bIsDead)
	{
		return;
	}
	if (!OnKillPayload.IsEmpty() && (OnKillChance <= 0.0f || FMath::FRand() * 100.0f < OnKillChance))
	{
		ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), OnKillPayload);
	}
	OnKill(const_cast<AActor*>(Payload.Target.Get()));
}

void UZL_Kit_Passive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding)
	{
		return;
	}
	bEnding = true;

	if (IsActive())
	{
		OnPassiveEnded();
	}
	AZeroLockCharacter* Hero = GetHero();
	if (DamageDealtHandle.IsValid() && Hero && Hero->GetMyAbilitySystemComp())
	{
		Hero->GetMyAbilitySystemComp()->OnDamageDealt.Remove(DamageDealtHandle);
	}
	DamageDealtHandle.Reset();
	if (ProcItem && Hero && Hero->GetItemProcs())
	{
		Hero->GetItemProcs()->RemoveItem(ProcItem);
	}
	ProcItem = nullptr;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (StatsHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(StatsHandle);
		}
		if (bTagsAdded)
		{
			ASC->RemoveLooseGameplayTags(PermanentTags);
		}
	}
	StatsHandle.Invalidate();
	bTagsAdded = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
