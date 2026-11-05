//Copyright Preetham Mukundan (C) 2026


#include "GAS/BaseCharAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GAS/BaseCharAttributeSet.h"
#include "GAS/BaseGameplayAbility.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_GE_BaseCooldown.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Items/ZL_ItemProcComponent.h"
#include "GAS/Tasks/ZL_Task_WaitSlamLocation.h"
#include "Telemetry/ZL_TelemetrySubsystem.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "ZeroLock/ZeroLock.h"

namespace
{
	/** Stacked debuff resist stops here, so a status always lasts a little. */
	constexpr float MaxDebuffResist = 90.0f;
}

void UBaseCharAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	
	if (OnNewAbilityAdded.IsBound())
	{
		OnNewAbilityAdded.Broadcast(AbilitySpec);
	}
	
}


void UBaseCharAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		OnNewAbilityAdded.Broadcast(Spec);
	}
}

float UBaseCharAbilitySystemComponent::GetEffectDurationScale(const FGameplayEffectSpec& Spec) const
{
	if (!Spec.Def || Spec.Def->DurationPolicy != EGameplayEffectDurationType::HasDuration)
	{
		return 1.0f;
	}

	const FGameplayEffectContextHandle Context = Spec.GetContext();
	const UAbilitySystemComponent* Source = Context.GetInstigatorAbilitySystemComponent();
	if (!Source)
	{
		return 1.0f;
	}

	float Scale = 1.0f;

	// Cooldowns come from an ability too; charge recharges have none.
	const UGameplayAbility* Ability = Context.GetAbility();
	if (Ability && Ability->GetCooldownGameplayEffect() != Spec.Def)
	{
		const UBaseGameplayAbility* Instance = Cast<UBaseGameplayAbility>(Context.GetAbilityInstance_NotReplicated());
		const float Imbued = Instance ? Instance->GetImbueBonus(EZL_ItemStat::AbilityDuration) : 0.0f;
		Scale *= 1.0f + (Source->GetNumericAttribute(UBaseCharAttributeSet::GetDurationExtensionAttribute()) + Imbued) / 100.0f;
	}

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActor());
	AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Source->GetAvatarActor());
	if (Source != this && Hero && Villan && !Hero->IsOnSameTeam(Villan))
	{
		const float Resist = FMath::Clamp(GetNumericAttribute(UBaseCharAttributeSet::GetDebufReductionAttribute()), 0.0f, MaxDebuffResist);
		Scale *= 1.0f - Resist / 100.0f;
	}
	return Scale;
}

FActiveGameplayEffectHandle UBaseCharAbilitySystemComponent::ApplyGameplayEffectSpecToSelf(const FGameplayEffectSpec& GameplayEffect, FPredictionKey PredictionKey)
{
	if (HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_UNSTOPPABLE))
	{
		FGameplayTagContainer GrantedTags;
		GameplayEffect.GetAllGrantedTags(GrantedTags);
		if (GrantedTags.HasAny(ZLItemStatus::GetCrowdControlTags()))
		{
			return FActiveGameplayEffectHandle();
		}
	}

	const float Scale = GetEffectDurationScale(GameplayEffect);
	if (FMath::IsNearlyEqual(Scale, 1.0f))
	{
		return Super::ApplyGameplayEffectSpecToSelf(GameplayEffect, PredictionKey);
	}

	FGameplayEffectSpec ScaledSpec(GameplayEffect);
	ScaledSpec.SetDuration(GameplayEffect.GetDuration() * Scale, true);
	return Super::ApplyGameplayEffectSpecToSelf(ScaledSpec, PredictionKey);
}

void UBaseCharAbilitySystemComponent::MarkInCombat()
{
	if (const UWorld* World = GetWorld())
	{
		LastCombatTime = World->GetTimeSeconds();
	}
}

float UBaseCharAbilitySystemComponent::GetTimeOutOfCombat() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() - LastCombatTime : 0.0f;
}

void UBaseCharAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	Super::NotifyAbilityActivated(Handle, Ability);

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UZL_TelemetrySubsystem* Telemetry = UZL_TelemetrySubsystem::Get(this))
		{
			Telemetry->RecordAbilityUse(Cast<AZeroLockCharacter>(GetAvatarActor()), Ability);
		}

		const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		const int32 InputID = Spec ? Spec->InputID : static_cast<int32>(EGASAbilityInputID::None);
		const int32 FirstItem = static_cast<int32>(EGASAbilityInputID::Item_1);

		// A stealth item is one of these, and its own cast mustn't end it. Zooming isn't an attack.
		if (InputID != static_cast<int32>(EGASAbilityInputID::None) && InputID != static_cast<int32>(EGASAbilityInputID::AltFire) && InputID < FirstItem)
		{
			BreakStealth();
		}

		const bool bUltimate = InputID == static_cast<int32>(EGASAbilityInputID::Ultimate);
		const bool bCast = bUltimate || InputID >= FirstItem
			|| InputID == static_cast<int32>(EGASAbilityInputID::Secondry_Attack)
			|| InputID == static_cast<int32>(EGASAbilityInputID::Ability_1)
			|| InputID == static_cast<int32>(EGASAbilityInputID::Ability_2);
		const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActor());
		if (bCast && Hero && Hero->GetItemProcs())
		{
			Hero->GetItemProcs()->NotifyEvent(EZL_ProcTrigger::AbilityCast, nullptr, 0.0f);
			Hero->GetItemProcs()->NotifyEvent(EZL_ProcTrigger::ImbuedCast, nullptr, 0.0f, InputID);
			if (bUltimate)
			{
				Hero->GetItemProcs()->NotifyEvent(EZL_ProcTrigger::UltimateCast, nullptr, 0.0f);
			}
		}
	}
}

float UBaseCharAbilitySystemComponent::ScaleDamageForProcs(EZL_ProcTrigger Trigger, const UAbilitySystemComponent* TargetASC, float DamageValue) const
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActor());
	AZeroLockCharacter* Target = TargetASC ? Cast<AZeroLockCharacter>(TargetASC->GetAvatarActor()) : nullptr;
	UZL_ItemProcComponent* Procs = Hero ? Hero->GetItemProcs() : nullptr;
	if (!Procs || !Target)
	{
		return DamageValue;
	}

	float Scale = Procs->GetDamageScale(Trigger, Target);
	if (Trigger == EZL_ProcTrigger::MeleeHit && bHeavyMeleeStrike)
	{
		Scale *= Procs->GetDamageScale(EZL_ProcTrigger::HeavyMeleeHit, Target);
	}
	if (Trigger == EZL_ProcTrigger::BulletHit && bHeadshotHit)
	{
		Scale *= Procs->GetDamageScale(EZL_ProcTrigger::Headshot, Target);
	}
	return DamageValue * Scale;
}

void UBaseCharAbilitySystemComponent::FinishHit(EZL_ProcTrigger HitTrigger, EZL_ProcTrigger TakenTrigger, const UAbilitySystemComponent* TargetASC, float DamageDealt, const UObject* DamageSource)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActor());
	AZeroLockCharacter* Target = TargetASC ? Cast<AZeroLockCharacter>(TargetASC->GetAvatarActor()) : nullptr;
	if (!Hero || !Target || DamageDealt <= 0.0f)
	{
		return;
	}

	BreakStealth();

	if (UZL_ItemProcComponent* Procs = Hero->GetItemProcs())
	{
		Procs->NotifyEvent(HitTrigger, Target, DamageDealt);
		if (HitTrigger == EZL_ProcTrigger::MeleeHit && bHeavyMeleeStrike)
		{
			Procs->NotifyEvent(EZL_ProcTrigger::HeavyMeleeHit, Target, DamageDealt);
		}
		if (HitTrigger == EZL_ProcTrigger::BulletHit && bHeadshotHit)
		{
			Procs->NotifyEvent(EZL_ProcTrigger::Headshot, Target, DamageDealt);
		}

		const UGameplayAbility* SourceAbility = Cast<UGameplayAbility>(DamageSource);
		const FGameplayAbilitySpec* Spec = SourceAbility ? FindAbilitySpecFromHandle(SourceAbility->GetCurrentAbilitySpecHandle()) : nullptr;
		if (Spec && Spec->InputID != static_cast<int32>(EGASAbilityInputID::None))
		{
			Procs->NotifyEvent(EZL_ProcTrigger::ImbuedHit, Target, DamageDealt, Spec->InputID);
		}
	}
	if (UZL_ItemProcComponent* Procs = Target->GetItemProcs())
	{
		if (TakenTrigger != EZL_ProcTrigger::None)
		{
			Procs->NotifyEvent(TakenTrigger, Hero, DamageDealt);
		}
		Procs->NotifyEvent(EZL_ProcTrigger::DamageTaken, Hero, DamageDealt);
	}
}

void UBaseCharAbilitySystemComponent::BreakStealth()
{
	if (HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_STEALTHED))
	{
		RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ZerolockGameplayTagsForBinding::TAG_STATUS_STEALTHED));
	}
}

void UBaseCharAbilitySystemComponent::CleanseDebuffs()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActor());
	if (!Hero)
	{
		return;
	}

	for (const FActiveGameplayEffectHandle& Handle : GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Effect = GetActiveGameplayEffect(Handle);
		const UAbilitySystemComponent* Source = Effect ? Effect->Spec.GetEffectContext().GetInstigatorAbilitySystemComponent() : nullptr;
		AZeroLockCharacter* Villan = Source ? Cast<AZeroLockCharacter>(Source->GetAvatarActor()) : nullptr;
		if (Villan && Source != this && Effect->GetDuration() > 0.0f && !Hero->IsOnSameTeam(Villan))
		{
			RemoveActiveGameplayEffect(Handle);
		}
	}
}

void UBaseCharAbilitySystemComponent::LevelUpAbility(UGameplayAbility* AbilityToUpgrade, int32 mLevel)
{
	ZLOG("AbilityUpgrade clicked");

	if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityToUpgrade->GetClass()))
	{
		AbilitySpec->Level = mLevel;
    
		// 1. Mark for replication
		MarkAbilitySpecDirty(*AbilitySpec);

		// 2. Update active instances (if any exist)
		TArray<UGameplayAbility*> Instances = AbilitySpec->GetAbilityInstances();
		for (UGameplayAbility* Instance : Instances)
		{
			if (UBaseGameplayAbility* BaseAbilityInstance = Cast<UBaseGameplayAbility>(Instance))
			{

			}
		}
		
		OnAbilityUpgraded.Broadcast(AbilityToUpgrade, AbilitySpec->Level);
	}
}

void UBaseCharAbilitySystemComponent::ApplyWeaponDamage(UAbilitySystemComponent* TargetASC, float DamageValue, const UObject* DamageSource)
{
	if (!TargetASC || !GE_WeaponClass)return;
	if (IsUntouchable(TargetASC)) return;
	if (!GetOwner()->HasAuthority()) return;
	if (TargetASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_BULLET_IMMUNE)) return;
	DamageValue = ScaleDamageForProcs(EZL_ProcTrigger::BulletHit, TargetASC, DamageValue);
	LastDamageDealt = 0.f;
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GE_WeaponClass, 1.f, Context);

	if (!SpecHandle.IsValid())	return;

	//Setting the base damage value 
	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Zerolock.DamageCalc.Weapon",false), DamageValue);
	//Apply the effect 
	// Telemetry reads this in PostGameplayEffectExecute, which runs inside the apply. Without DamageSource, a name the caller set stays.
	TGuardValue<FString> TelemetrySourceGuard(TelemetryDamageSource, DamageSource ? UZL_TelemetrySubsystem::CleanClassName(DamageSource->GetClass()) : TelemetryDamageSource);
	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

	//Sending event to the source
	

	FGameplayTag TagToSend =FGameplayTag::RequestGameplayTag("Event.WeaponHit",false);
	const float DamageDealt = LastDamageDealt;
	SendGameplayEventToSelf(TagToSend, TargetASC, DamageDealt);
	TagToSend = FGameplayTag::RequestGameplayTag("Event.WeaponRecieved",false);
	SendGameplayEventToTarget(TagToSend, TargetASC);
	FinishHit(EZL_ProcTrigger::BulletHit, EZL_ProcTrigger::BulletTaken, TargetASC, DamageDealt, DamageSource);

	//UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActor(),FGameplayTag::RequestGameplayTag("Event.WeaponHit",false),WeaponHitEventData);
	
}

void UBaseCharAbilitySystemComponent::ApplySpiritDamage(UAbilitySystemComponent* TargetASC, float DamageValue, const UObject* DamageSource)
{
	if (!TargetASC || !GE_SpiritClass)return;
	if (IsUntouchable(TargetASC)) return;
	if (!GetOwner()->HasAuthority()) return;
	DamageValue = ScaleDamageForProcs(EZL_ProcTrigger::SpiritHit, TargetASC, DamageValue);
	LastDamageDealt = 0.f;
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GE_SpiritClass, 1.f, Context);

	if (!SpecHandle.IsValid())
		return;

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Zerolock.DamageCalc.Spirit",false), DamageValue);

	// Telemetry reads this in PostGameplayEffectExecute, which runs inside the apply. Without DamageSource, a name the caller set stays.
	TGuardValue<FString> TelemetrySourceGuard(TelemetryDamageSource, DamageSource ? UZL_TelemetrySubsystem::CleanClassName(DamageSource->GetClass()) : TelemetryDamageSource);
	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

	FGameplayTag TagToSend =FGameplayTag::RequestGameplayTag("Event.SpiritHit",false);
	const float DamageDealt = LastDamageDealt;
	SendGameplayEventToSelf(TagToSend, TargetASC, DamageDealt);
	TagToSend = FGameplayTag::RequestGameplayTag("Event.SpiritRecieved",false);
	SendGameplayEventToTarget(TagToSend, TargetASC);
	FinishHit(EZL_ProcTrigger::SpiritHit, EZL_ProcTrigger::SpiritTaken, TargetASC, DamageDealt, DamageSource);
}

void UBaseCharAbilitySystemComponent::ApplyMeleeDamage(UAbilitySystemComponent* TargetASC, float DamageValue, const UObject* DamageSource)
{
	if (!TargetASC || !GE_MeleeClass)return;
	if (IsUntouchable(TargetASC)) return;
	if (!GetOwner()->HasAuthority()) return;
	DamageValue = ScaleDamageForProcs(EZL_ProcTrigger::MeleeHit, TargetASC, DamageValue);
	LastDamageDealt = 0.f;
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GE_MeleeClass, 1.f, Context);

	if (!SpecHandle.IsValid())
		return;

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Zerolock.DamageCalc.Melee",false), DamageValue);

	// Telemetry reads this in PostGameplayEffectExecute, which runs inside the apply. Without DamageSource, a name the caller set stays.
	TGuardValue<FString> TelemetrySourceGuard(TelemetryDamageSource, DamageSource ? UZL_TelemetrySubsystem::CleanClassName(DamageSource->GetClass()) : TelemetryDamageSource);
	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

	FGameplayTag TagToSend =FGameplayTag::RequestGameplayTag("Event.MeleeHit",false);
	const float DamageDealt = LastDamageDealt;
	SendGameplayEventToSelf(TagToSend, TargetASC, DamageDealt);
	TagToSend = FGameplayTag::RequestGameplayTag("Event.MeleeRecieved",false);
	SendGameplayEventToTarget(TagToSend, TargetASC);
	FinishHit(EZL_ProcTrigger::MeleeHit, EZL_ProcTrigger::None, TargetASC, DamageDealt, DamageSource);
}

void UBaseCharAbilitySystemComponent::AddCombatBarrier(float Amount, float Duration)
{
	if (!IsOwnerActorAuthoritative() || Amount <= 0.0f)
	{
		return;
	}
	const FGameplayAttribute Barrier = UBaseCharAttributeSet::GetCombatBarrierAttribute();
	SetNumericAttributeBase(Barrier, GetNumericAttributeBase(Barrier) + Amount);
	if (Duration <= 0.0f || !GetWorld())
	{
		return;
	}
	// Damage eats the newest and oldest barrier alike, so on expiry take back up to Amount of what's left.
	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Amount, Generation = CombatBarrierGeneration]()
	{
		if (Generation != CombatBarrierGeneration)
		{
			return;
		}
		const FGameplayAttribute Barrier = UBaseCharAttributeSet::GetCombatBarrierAttribute();
		const float Current = GetNumericAttributeBase(Barrier);
		SetNumericAttributeBase(Barrier, Current - FMath::Min(Current, Amount));
	}), Duration, false);
}

void UBaseCharAbilitySystemComponent::ClearCombatBarrier()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}
	++CombatBarrierGeneration;
	SetNumericAttributeBase(UBaseCharAttributeSet::GetCombatBarrierAttribute(), 0.0f);
}

void UBaseCharAbilitySystemComponent::ApplyHeal(UAbilitySystemComponent* TargetASC, float HealValue)
{
	if (!TargetASC || !GE_HealingClass)return;

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GE_HealingClass, 1.f, Context);

	if (!SpecHandle.IsValid())
		return;

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Zerolock.HealCalc.Healing",false), HealValue);

	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

	FGameplayTag TagToSend =FGameplayTag::RequestGameplayTag("Event.HealHit",false);
	SendGameplayEventToSelf(TagToSend, TargetASC);
	TagToSend = FGameplayTag::RequestGameplayTag("Event.HealRecieved",false);
}

void UBaseCharAbilitySystemComponent::ApplyGameplayEffect(UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> EffectClass, int32 level)
{

	if (!TargetASC || !EffectClass)return;
	
	if (IsUntouchable(TargetASC)) return;

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(EffectClass, level, Context);

	if (!SpecHandle.IsValid())
		return;
	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

}

void UBaseCharAbilitySystemComponent::ApplyGameplayEffectWithStacks(UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> EffectClass, int32 level, int32 StackNum)
{
	
	if (!TargetASC || !EffectClass)return;
	
	if (IsUntouchable(TargetASC)) return;

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(EffectClass, level, Context);

	if (!SpecHandle.IsValid())
		return;
	SpecHandle.Data.Get()->SetStackCount(StackNum);
	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

}

void UBaseCharAbilitySystemComponent::Client_SendGameplayEventToOwner_Implementation(FGameplayTag Tag,
	FGameplayEventData Payload)
{
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActor(), Tag, Payload);
}

bool UBaseCharAbilitySystemComponent::IsUntouchable(UAbilitySystemComponent* TargetToCheck)
{
	 if ( TargetToCheck->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Zerolock.Untouchable",false)))
	 {
	 	SendGameplayEventToTarget(FGameplayTag::RequestGameplayTag("Event.UntouchableTrigger",false),TargetToCheck);
	 	return true;
	 }
	return false;
}


void UBaseCharAbilitySystemComponent::SendGameplayEventToSelf(FGameplayTag Tag, UAbilitySystemComponent* TargetASC, float Magnitude)
{

	FGameplayEventData EventDataToSend;
	EventDataToSend.EventMagnitude = Magnitude;
	EventDataToSend.Instigator = GetAvatarActor();
	EventDataToSend.EventTag =Tag;
	EventDataToSend.ContextHandle =MakeEffectContext();
	EventDataToSend.ContextHandle.AddSourceObject(GetAvatarActor());
	EventDataToSend.Instigator= GetAvatarActor();
	EventDataToSend.Target =TargetASC->GetAvatarActor();

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActor(),Tag,EventDataToSend);
	
}

void UBaseCharAbilitySystemComponent::SendGameplayEventToTarget(FGameplayTag Tag, UAbilitySystemComponent* TargetASC)
{
	FGameplayEventData EventDataToSend;
	EventDataToSend.Instigator = GetAvatarActor();
	EventDataToSend.EventTag =Tag;
	EventDataToSend.ContextHandle =MakeEffectContext();
	EventDataToSend.ContextHandle.AddSourceObject(GetAvatarActor());
	EventDataToSend.Instigator= GetAvatarActor();
	EventDataToSend.Target =TargetASC->GetAvatarActor();

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetASC->GetAvatarActor(),Tag,EventDataToSend);
	
	if (GetOwner()->HasAuthority())
	{
		APawn* TargetPawn = Cast<APawn>(TargetASC->GetAvatarActor());
		if (TargetPawn && !TargetPawn->IsLocallyControlled())
		{
			if (UBaseCharAbilitySystemComponent* TargetBaseASC = Cast<UBaseCharAbilitySystemComponent>(TargetASC))
			{
				TargetBaseASC->Client_SendGameplayEventToOwner(Tag, EventDataToSend);
			}
		}
	}
	
}

void UBaseCharAbilitySystemComponent::AdjustActiveEffectsDurationByPercentage( float Multiplier)
{
	if (GetOwnerRole() != ROLE_Authority) return;

	FGameplayTag AbilityTag  = FGameplayTag::RequestGameplayTag("ZerolockAbilities.Cooldown",false);
	FGameplayTag InputTag  = FGameplayTag::RequestGameplayTag("Zerolock.InputBindTags",false);
	for (FActiveGameplayEffectHandle Handle : ActiveGameplayEffects.GetAllActiveEffectHandles())
	{
		FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);


		if (ActiveGE && ActiveGE->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
		{
			for (auto tagssss : ActiveGE->Spec.CapturedSourceTags.GetAggregatedTags()->GetGameplayTagArray())
			{
				ZLOG(tagssss.ToString());
			}
			if (ActiveGE->Spec.CapturedSourceTags.GetAggregatedTags()->HasTag(AbilityTag) || ActiveGE->Spec.CapturedSourceTags.GetAggregatedTags()->HasTag(InputTag))
			{
				float CurrentDuration = ActiveGE->Spec.Duration;
				ActiveGE->Spec.Duration = FMath::Max(CurrentDuration * Multiplier, SMALL_NUMBER);

				
				ActiveGameplayEffects.MarkItemDirty(*ActiveGE);
				ActiveGameplayEffects.CheckDuration(Handle);
			}
		}
	}
}

void UBaseCharAbilitySystemComponent::AdjustActiveEffectsDurationByValue(float reductionAmount)
{
	if (GetOwnerRole() != ROLE_Authority) return;

	FGameplayTag AbilityTag  = FGameplayTag::RequestGameplayTag("ZerolockAbilities.Cooldown",false);
	FGameplayTag InputTag  = FGameplayTag::RequestGameplayTag("Zerolock.InputBindTags",false);
	for (FActiveGameplayEffectHandle Handle : ActiveGameplayEffects.GetAllActiveEffectHandles())
	{
		FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);


		if (ActiveGE && ActiveGE->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
		{
			for (auto tagssss : ActiveGE->Spec.CapturedSourceTags.GetAggregatedTags()->GetGameplayTagArray())
			{
				ZLOG(tagssss.ToString());
			}
			if (ActiveGE->Spec.CapturedSourceTags.GetAggregatedTags()->HasTag(AbilityTag) || ActiveGE->Spec.CapturedSourceTags.GetAggregatedTags()->HasTag(InputTag))
			{
				float CurrentDuration = ActiveGE->Spec.Duration;
				ActiveGE->Spec.Duration = FMath::Max(CurrentDuration - reductionAmount, SMALL_NUMBER);

				
				ActiveGameplayEffects.MarkItemDirty(*ActiveGE);
				ActiveGameplayEffects.CheckDuration(Handle);
			}
		}
	}
}

FActiveGameplayEffect* UBaseCharAbilitySystemComponent::GetActiveGameplayEffect_Mutable(
	FActiveGameplayEffectHandle Handle)
{
	return ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
}

TArray<FActiveGameplayEffectHandle> UBaseCharAbilitySystemComponent::GetAllActiveEffectHandles() const
{
	return ActiveGameplayEffects.GetAllActiveEffectHandles();
}

void UBaseCharAbilitySystemComponent::RegisterPendingSlamTask(const FPredictionKey& Key,
	UZL_Task_WaitSlamLocation* Task)
{
	PendingSlamTasks.Add(Key, Task);
}

void UBaseCharAbilitySystemComponent::UnregisterPendingSlamTask(const FPredictionKey& Key)
{
	PendingSlamTasks.Remove(Key);
}


void UBaseCharAbilitySystemComponent::ServerReportSlamLocation_Implementation(FPredictionKey ActivationKey,
                                                                              FVector Location)
{
	if (TWeakObjectPtr<UZL_Task_WaitSlamLocation>* Found = PendingSlamTasks.Find(ActivationKey))
	{
		if (UZL_Task_WaitSlamLocation* Task = Found->Get())
		{
			Task->ReceiveLocationFromClient(Location);
		}
		PendingSlamTasks.Remove(ActivationKey);
	}
	else
	{
		CachedSlamLocations.Add(ActivationKey, Location);
	}
}

void UBaseCharAbilitySystemComponent::MarkActiveGameplayEffectDirty(FActiveGameplayEffect* ActiveGE)
{
	if (ActiveGE)
	{
		ActiveGameplayEffects.MarkItemDirty(*ActiveGE);
	}
}

void UBaseCharAbilitySystemComponent::CheckActiveEffectDuration(const FActiveGameplayEffectHandle& Handle)
{
	ActiveGameplayEffects.CheckDuration(Handle);
}

bool UBaseCharAbilitySystemComponent::TryConsumeClientReplicatedTargetData(FGameplayAbilitySpecHandle AbilityHandle,
	FPredictionKey AbilityOriginalPredictionKey)
{
	TSharedPtr<FAbilityReplicatedDataCache> CachedData = AbilityTargetDataMap.Find(FGameplayAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData.IsValid())
	{
		const bool bConsumed = CachedData->TargetData.Num() > 0;
		CachedData->TargetData.Clear();
		CachedData->bTargetConfirmed = false;
		CachedData->bTargetCancelled = false;
		return bConsumed;
	}
	return false;
}


void UBaseCharAbilitySystemComponent::ConsumeAbilityCharge(UBaseGameplayAbility* Ability, const FGameplayAttribute& ChargeAttr,
	const FGameplayAttribute& MaxChargeAttr, FGameplayTag RechargeTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Ability || !ChargeAttr.IsValid() || !RechargeTag.IsValid()) return;

	ApplyModToAttribute(ChargeAttr, EGameplayModOp::Additive, -1.0f);

	FZLChargeRecharge& Recharge = ChargeRecharges.FindOrAdd(RechargeTag);
	Recharge.Ability = Ability;
	Recharge.ChargeAttr = ChargeAttr;
	Recharge.MaxChargeAttr = MaxChargeAttr;

	// Already recharging: the running timer picks this charge up when it finishes.
	if (!GetActiveGameplayEffect(Recharge.Handle))
	{
		StartChargeRecharge(RechargeTag);
	}
}

void UBaseCharAbilitySystemComponent::StartChargeRecharge(FGameplayTag RechargeTag)
{
	FZLChargeRecharge* Recharge = ChargeRecharges.Find(RechargeTag);
	UBaseGameplayAbility* Ability = Recharge ? Recharge->Ability.Get() : nullptr;
	if (!Ability) return;

	// Read each time, so cooldown reduction and ability level changes apply to the next charge.
	const float Duration = Ability->GetCoolDownTime();
	if (Duration <= 0.f)
	{
		// A 0 duration GE is instant and would never expire: just refill.
		SetNumericAttributeBase(Recharge->ChargeAttr, GetNumericAttribute(Recharge->MaxChargeAttr));
		return;
	}

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());
	FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(UZL_GE_BaseCooldown::StaticClass(), Ability->GetAbilityLevel(), Context);
	if (!Spec.IsValid()) return;

	Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_COOLDOWN_DURATION, Duration);
	Spec.Data->DynamicGrantedTags.AddTag(RechargeTag);
	Recharge->Handle = ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	if (FOnActiveGameplayEffectRemoved_Info* OnRemoved = OnGameplayEffectRemoved_InfoDelegate(Recharge->Handle))
	{
		OnRemoved->AddUObject(this, &ThisClass::OnChargeRechargeRemoved, RechargeTag);
	}
}

void UBaseCharAbilitySystemComponent::OnChargeRechargeRemoved(const FGameplayEffectRemovalInfo& RemovalInfo, FGameplayTag RechargeTag)
{
	FZLChargeRecharge* Recharge = ChargeRecharges.Find(RechargeTag);
	if (!Recharge) return;
	Recharge->Handle = FActiveGameplayEffectHandle();

	// Removed by something else (cleanup, a cleanse): no charge, and no new timer.
	if (RemovalInfo.bPrematureRemoval) return;

	ApplyModToAttribute(Recharge->ChargeAttr, EGameplayModOp::Additive, 1.0f);

	if (GetNumericAttribute(Recharge->ChargeAttr) < GetNumericAttribute(Recharge->MaxChargeAttr))
	{
		StartChargeRecharge(RechargeTag);
	}
}