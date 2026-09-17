//Copyright Preetham Mukundan (C) 2026


#include "GAS/BaseCharAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Zero_BasePlayerController.h"
#include "Zero_BasePlayerState.h"
#include "Chaos/Deformable/MuscleActivationConstraints.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "ZeroLock/ZeroLockCharacter.h"

UBaseCharAttributeSet::UBaseCharAttributeSet()
{
	MaximumHealth = 0.0f;
	CurrentHealth = 0.0f;
	HealthRegeneration = 0.0f;
	HealingBonus = 0.0f;
	HealingReduction = 0.0f;
	CurrentSpeed = 0.0f;
	CurrentJump = 0.0f;
	Soul =0;
	FireRate = 0.0f;
	WeaponDamage = 0.0f;
	CurrentAmmo = 0.0f;
	MaxAmmo = 0.0f;
	WeaponResistance=0.0f;
	WeaponResistanceReduction =0.0f;
	FlatWeapon =0.0f;
	WeaponLifeSteal = 0.0f;
	SpiritDamage = 0.0f;
	SpiritResistance = 0.0f;
	SpiritResistanceReduction = 0.0f;
	FlatSpirit=0.0f;
	SpiritLifeSteal =0.0f;
	MeleeDamage = 0.0f;
	MeleeResistance = 0.0f;
	MeleeResistanceReduction = 0.0f;
	MeleeLifeSteal = 0.0f;
	
}
void UBaseCharAttributeSet::PreAttributeChange(const FGameplayAttribute & Attribute, float & NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaximumHealthAttribute())
	{
		AdjustAttributeForMaxChange(CurrentHealth, MaximumHealth, NewValue, GetCurrentHealthAttribute());
	}

}

void UBaseCharAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData & Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* Source = Context.GetOriginalInstigatorAbilitySystemComponent();
	const FGameplayTagContainer& SourceTags = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();
	FGameplayTagContainer SpecAssetTags;
	Data.EffectSpec.GetAllAssetTags(SpecAssetTags);

	// Get the Target actor, which should be our owner
	AActor* TargetActor = nullptr;
	AController* TargetController = nullptr;
	AZeroLockCharacter* TargetCharacter = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		TargetCharacter = Cast<AZeroLockCharacter>(TargetActor);
	}

	// Get the Source actor
	AActor* SourceActor = nullptr;
	AController* SourceController = nullptr;
	AZeroLockCharacter* SourceCharacter = nullptr;
	if (Source && Source->AbilityActorInfo.IsValid() && Source->AbilityActorInfo->AvatarActor.IsValid())
	{
		SourceActor = Source->AbilityActorInfo->AvatarActor.Get();
		SourceController = Source->AbilityActorInfo->PlayerController.Get();
		if (SourceController == nullptr && SourceActor != nullptr)
		{
			if (APawn* Pawn = Cast<APawn>(SourceActor))
			{
				SourceController = Pawn->GetController();
			}
		}

		// Use the controller to find the source pawn
		if (SourceController)
		{
			SourceCharacter = Cast<AZeroLockCharacter>(SourceController->GetPawn());
		}
		else
		{
			SourceCharacter = Cast<AZeroLockCharacter>(SourceActor);
		}
	}
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		AZero_BasePlayerState* Target_PS = TargetCharacter->GetPlayerState<AZero_BasePlayerState>();
		AZero_BasePlayerState* Source_PS = SourceCharacter->GetPlayerState<AZero_BasePlayerState>();
		if (Target_PS && Source_PS)
		{
			if (Target_PS->TeamID != Source_PS->TeamID)
			{
				const float LocalDamageDone = GetDamage();
				SetDamage(0.f);
				if (LocalDamageDone > 0.0f)
				{
					bool wasAlive = true;
					if (TargetCharacter)
					{
						wasAlive = TargetCharacter->IsAlive();
					}
					
					if (wasAlive && TargetCharacter)
					{
						if (AZero_BasePlayerController* MyPC = Cast<AZero_BasePlayerController>(SourceCharacter->GetController()))
						{
							MyPC->ShowDamageNumber(LocalDamageDone,TargetCharacter,Data.EffectSpec.GetDynamicAssetTags());
						}
						const float NewHealth = GetCurrentHealth() - LocalDamageDone;
						SetCurrentHealth(FMath::Clamp(NewHealth, 0.0f, GetMaximumHealth()));	
						TargetCharacter->AddLastHit(SourceCharacter);
						if (!TargetCharacter->IsAlive())
						{
							//TargetDeath
							TargetCharacter->HandleDeath();
						}
				
					}
				}
			}
		}
		
	}
	else if (Data.EvaluatedData.Attribute == GetMaximumHealthAttribute())
	{
		//TargetCharacter->HealthChanged(GetCurrentHealth(),GetMaximumHealth());
		
	}

	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		// Store a local copy of the amount of Healing done and clear the Healing attribute.
		const float LocalHealingDone = GetHealing();

		SetHealing(0.f);

		if (LocalHealingDone > 0.0f)
		{
			// Apply the Health change and then clamp it.
			const float NewHealth = GetCurrentHealth() + LocalHealingDone;

			SetCurrentHealth(FMath::Clamp(NewHealth, 0.0f, GetMaximumHealth()));
			//TargetCharacter->HealthChanged(GetCurrentHealth(),GetMaximumHealth());
		}
	}

	else if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		SetCurrentHealth(FMath::Clamp(GetCurrentHealth(), 0.0f, GetMaximumHealth()));
		//TargetCharacter->HealthChanged(GetCurrentHealth(),GetMaximumHealth());
	}

	else if (Data.EvaluatedData.Attribute == GetHealthRegenerationAttribute())
	{
		SetHealthRegeneration(FMath::Clamp(GetHealthRegeneration(), 0.0f, GetMaximumHealth()));
	}
	if (Data.EvaluatedData.Attribute == GetAbilityCharges_1Attribute()) 
		SetAbilityCharges_1(FMath::Clamp(GetAbilityCharges_1(), 0.0f, GetMaxCharges_1()));
	
	if (Data.EvaluatedData.Attribute == GetAbilityCharges_2Attribute()) 
		SetAbilityCharges_2(FMath::Clamp(GetAbilityCharges_2(), 0.0f, GetMaxCharges_2()));
	
	if (Data.EvaluatedData.Attribute == GetAbilityCharges_3Attribute()) 
		SetAbilityCharges_3(FMath::Clamp(GetAbilityCharges_3(), 0.0f, GetMaxCharges_3()));
	
	if (Data.EvaluatedData.Attribute == GetAbilityCharges_4Attribute()) 
		SetAbilityCharges_4(FMath::Clamp(GetAbilityCharges_4(), 0.0f, GetMaxCharges_4()));
}

void UBaseCharAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);


	AZeroLockCharacter* TargetChar = Cast<AZeroLockCharacter>(GetOwningActor());
	if (!TargetChar)
	{
		return;
	}
	if (Attribute == GetCurrentSpeedAttribute())
	{
		//TargetChar->GetCharacterMovement()->MaxWalkSpeed = NewValue;
	
	}

	else if (Attribute == GetCurrentJumpAttribute())
	{
	
		//TargetChar->GetCharacterMovement()->JumpZVelocity = NewValue;
	
	}
	else if (Attribute == GetFireRateAttribute())
	{
		TargetChar->ChangeFireRate();
	}
	else if (Attribute == GetMaximumHealthAttribute() )
	{
		//TargetChar->HealthChanged(GetCurrentHealth(),GetMaximumHealth());
	}
	else if (Attribute == GetCurrentAmmoAttribute())
	{
		if (GetCurrentAmmo() <= 0.0f)
		{
			TargetChar->Reload();	
		}
	}
	
}

void UBaseCharAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, CurrentHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, MaximumHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, HealthRegeneration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,HealingReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,HealingBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, CurrentSpeed, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, CurrentJump, COND_None, REPNOTIFY_OnChanged);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, Soul, COND_None, REPNOTIFY_OnChanged);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, FireRate, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, WeaponDamage,COND_None,REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, FlatWeapon,COND_None,REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, CurrentAmmo, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, MaxAmmo, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, WeaponResistance, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, WeaponResistanceReduction, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,WeaponLifeSteal, COND_None, REPNOTIFY_OnChanged);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, SpiritDamage, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, SpiritResistance, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, SpiritResistanceReduction, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, FlatSpirit, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,SpiritLifeSteal, COND_None, REPNOTIFY_OnChanged);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,MeleeDamage, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,MeleeResistance, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,MeleeLifeSteal, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,MeleeResistanceReduction, COND_None, REPNOTIFY_OnChanged);

	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,CooldownReduction, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,DebufReduction, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet,DurationExtension, COND_None, REPNOTIFY_OnChanged);


	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, AbilityCharges_1, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, MaxCharges_1, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, AbilityCharges_2, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, MaxCharges_2, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, AbilityCharges_3, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, MaxCharges_3, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, AbilityCharges_4, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharAttributeSet, MaxCharges_4, COND_None, REPNOTIFY_Always);
}

void UBaseCharAttributeSet::OnRep_AbilityCharges_1(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, AbilityCharges_1, OldValue);
}

void UBaseCharAttributeSet::OnRep_MaxCharges_1(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, MaxCharges_1, OldValue);
}

void UBaseCharAttributeSet::OnRep_AbilityCharges_2(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, AbilityCharges_2, OldValue);
}

void UBaseCharAttributeSet::OnRep_MaxCharges_2(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, MaxCharges_2, OldValue);
}

void UBaseCharAttributeSet::OnRep_AbilityCharges_3(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, AbilityCharges_3, OldValue);
}

void UBaseCharAttributeSet::OnRep_MaxCharges_3(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, MaxCharges_3, OldValue);
}

void UBaseCharAttributeSet::OnRep_AbilityCharges_4(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, AbilityCharges_4, OldValue);
}

void UBaseCharAttributeSet::OnRep_MaxCharges_4(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, MaxCharges_4, OldValue);
}

void UBaseCharAttributeSet::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, const float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty) const
{
	UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent();

	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();

	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilitySystemComponent)
	{
		// Change current value to maintain the Current Value / Maximum Value percentage.
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		
		 float NewDelta= (CurrentMaxValue > 0.f) ? FMath::Clamp(CurrentValue,0.0,NewMaxValue) : NewMaxValue;
		if (NewMaxValue > CurrentMaxValue)
		{
			NewDelta = (CurrentMaxValue > 0.f) ? CurrentValue + (NewMaxValue - CurrentMaxValue) : NewMaxValue;
		}
		

		AbilitySystemComponent->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Override, NewDelta);
	}
}

void UBaseCharAttributeSet::OnRep_CurrentHealth(const FGameplayAttributeData & OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, CurrentHealth, OldValue);
}
void UBaseCharAttributeSet::OnRep_CurrentSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, CurrentSpeed, OldValue);
}
void UBaseCharAttributeSet::OnRep_CurrentJump(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, CurrentJump, OldValue);
}

void UBaseCharAttributeSet::OnRep_MaximumHealth(const FGameplayAttributeData & OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, MaximumHealth, OldValue);
}

void UBaseCharAttributeSet::OnRep_HealthRegeneration(const FGameplayAttributeData & OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, HealthRegeneration, OldValue);
}


void UBaseCharAttributeSet::OnRep_HealingReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, HealingReduction, OldValue);
}

void UBaseCharAttributeSet::OnRep_HealingBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, HealingBonus, OldValue);
}

void UBaseCharAttributeSet::OnRep_Souls(const FGameplayAttributeData & OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, Soul, OldValue);
}

void UBaseCharAttributeSet::OnRep_FireRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, FireRate, OldValue);
}

void UBaseCharAttributeSet::OnRep_WeaponDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, WeaponDamage, OldValue);
}

void UBaseCharAttributeSet::OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, CurrentAmmo, OldValue);
}

void UBaseCharAttributeSet::OnRep_MaxAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, MaxAmmo, OldValue);
}

void UBaseCharAttributeSet::OnRep_WeaponResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, WeaponResistance, OldValue);
}

void UBaseCharAttributeSet::OnRep_FlatWeapon(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,FlatWeapon,OldValue);
}

void UBaseCharAttributeSet::OnRep_WeaponLifeSteal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,WeaponLifeSteal, OldValue);
}


void UBaseCharAttributeSet::OnRep_FlatSpirit(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,FlatSpirit,OldValue);
}

void UBaseCharAttributeSet::OnRep_WeaponResistanceReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,WeaponResistanceReduction,OldValue);
}

void UBaseCharAttributeSet::OnRep_SpiritDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, SpiritDamage, OldValue);
}

void UBaseCharAttributeSet::OnRep_SpiritResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet, SpiritResistance, OldValue);
}

void UBaseCharAttributeSet::OnRep_SpiritResistanceReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,SpiritResistanceReduction, OldValue);
}

void UBaseCharAttributeSet::OnRep_SpiritLifeSteal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,SpiritLifeSteal, OldValue);
}

void UBaseCharAttributeSet::OnRep_MeleeDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,MeleeDamage, OldValue);
}

void UBaseCharAttributeSet::OnRep_MeleeResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,MeleeResistance, OldValue);
}

void UBaseCharAttributeSet::OnRep_MeleeLifeSteal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,MeleeLifeSteal, OldValue);
}

void UBaseCharAttributeSet::OnRep_MeleeResistanceReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,MeleeResistanceReduction, OldValue);
}

void UBaseCharAttributeSet::OnRep_CooldownReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,CooldownReduction, OldValue);
}

void UBaseCharAttributeSet::OnRep_DebufReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,DebufReduction, OldValue);
}

void UBaseCharAttributeSet::OnRep_DurationExtension(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharAttributeSet,DurationExtension, OldValue);
}
