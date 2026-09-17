//Copyright Preetham Mukundan (C) 2026


#include "GAS/BaseCharAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZeroLock/ZeroLock.h"

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

void UBaseCharAbilitySystemComponent::ApplyWeaponDamage(UAbilitySystemComponent* TargetASC, float DamageValue)
{
	if (!TargetASC || !GE_WeaponClass)return;
	if (IsUntouchable(TargetASC)) return;
	if (!GetOwner()->HasAuthority()) return;
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GE_WeaponClass, 1.f, Context);

	if (!SpecHandle.IsValid())	return;

	//Setting the base damage value 
	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Zerolock.DamageCalc.Weapon",false), DamageValue);
	//Apply the effect 
	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

	//Sending event to the source
	

	FGameplayTag TagToSend =FGameplayTag::RequestGameplayTag("Event.WeaponHit",false);
	SendGameplayEventToSelf(TagToSend, TargetASC);
	TagToSend = FGameplayTag::RequestGameplayTag("Event.WeaponRecieved",false);
	SendGameplayEventToTarget(TagToSend, TargetASC);

	//UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActor(),FGameplayTag::RequestGameplayTag("Event.WeaponHit",false),WeaponHitEventData);
	
}

void UBaseCharAbilitySystemComponent::ApplySpiritDamage(UAbilitySystemComponent* TargetASC, float DamageValue)
{
	if (!TargetASC || !GE_SpiritClass)return;
	if (IsUntouchable(TargetASC)) return;
	if (!GetOwner()->HasAuthority()) return;
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GE_SpiritClass, 1.f, Context);

	if (!SpecHandle.IsValid())
		return;

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Zerolock.DamageCalc.Spirit",false), DamageValue);

	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

	FGameplayTag TagToSend =FGameplayTag::RequestGameplayTag("Event.SpiritHit",false);
	SendGameplayEventToSelf(TagToSend, TargetASC);
	TagToSend = FGameplayTag::RequestGameplayTag("Event.SpiritRecieved",false);
	SendGameplayEventToTarget(TagToSend, TargetASC);
}

void UBaseCharAbilitySystemComponent::ApplyMeleeDamage(UAbilitySystemComponent* TargetASC, float DamageValue)
{
	if (!TargetASC || !GE_MeleeClass)return;
	if (IsUntouchable(TargetASC)) return;
	if (!GetOwner()->HasAuthority()) return;
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GE_MeleeClass, 1.f, Context);

	if (!SpecHandle.IsValid())
		return;

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Zerolock.DamageCalc.Melee",false), DamageValue);

	ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),TargetASC);

	FGameplayTag TagToSend =FGameplayTag::RequestGameplayTag("Event.MeleeHit",false);
	SendGameplayEventToSelf(TagToSend, TargetASC);
	TagToSend = FGameplayTag::RequestGameplayTag("Event.MeleeRecieved",false);
	SendGameplayEventToTarget(TagToSend, TargetASC);
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


void UBaseCharAbilitySystemComponent::SendGameplayEventToSelf(FGameplayTag Tag, UAbilitySystemComponent* TargetASC)
{

	FGameplayEventData EventDataToSend;
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
