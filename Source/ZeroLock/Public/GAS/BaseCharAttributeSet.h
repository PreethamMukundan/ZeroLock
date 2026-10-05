//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "BaseCharAttributeSet.generated.h"


#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UBaseCharAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
		
public:

	UBaseCharAttributeSet();

	// Attribute Set Overrides.
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// Set Attributes to replicate.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Used to create a local copy of Damage which is then subtracted from Current Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", meta = (HideFromLevelInfos))
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, Damage)

		// Used to create a local copy of Healing which is then added to Current Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", meta = (HideFromLevelInfos))
	FGameplayAttributeData Healing;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, Healing)

		// Holds the current value for Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_CurrentHealth)
	FGameplayAttributeData CurrentHealth;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, CurrentHealth)

		// Holds the value for Maximum Health.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_MaximumHealth)
	FGameplayAttributeData MaximumHealth;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MaximumHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_HealingReduction)
	FGameplayAttributeData HealingReduction;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, HealingReduction)

	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_HealingBonus)
	FGameplayAttributeData HealingBonus;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, HealingBonus)

		// Holds the value for Health Regeneration.
	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_HealthRegeneration)
	FGameplayAttributeData HealthRegeneration;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, HealthRegeneration)

	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_CurrentSpeed)
	FGameplayAttributeData CurrentSpeed;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, CurrentSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "Health Attribute Set", ReplicatedUsing = OnRep_CurrentJump)
	FGameplayAttributeData CurrentJump;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, CurrentJump)

	UPROPERTY(BlueprintReadOnly, Category = "Soul Attribute Set", ReplicatedUsing = OnRep_Souls)
	FGameplayAttributeData Soul;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, Soul)

	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_FireRate)
	FGameplayAttributeData FireRate;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, FireRate)

	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_WeaponDamage)
	FGameplayAttributeData WeaponDamage;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, WeaponDamage)

	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_WeaponResistance)
	FGameplayAttributeData WeaponResistance;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, WeaponResistance)

	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_FlatWeapon)
	FGameplayAttributeData FlatWeapon;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, FlatWeapon)

	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_WeaponResistanceReduction)
	FGameplayAttributeData WeaponResistanceReduction;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, WeaponResistanceReduction)
	
	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_WeaponLifeSteal)
	FGameplayAttributeData WeaponLifeSteal;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, WeaponLifeSteal)

	
	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_CurrentAmmo)
	FGameplayAttributeData CurrentAmmo;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, CurrentAmmo)

	
	UPROPERTY(BlueprintReadOnly,Category = "Weapon Attribute Set", ReplicatedUsing= OnRep_MaxAmmo)
	FGameplayAttributeData MaxAmmo;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MaxAmmo)

	UPROPERTY(BlueprintReadOnly,Category = "Spirit Attribute Set", ReplicatedUsing= OnRep_SpiritDamage)
	FGameplayAttributeData SpiritDamage;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, SpiritDamage)

	UPROPERTY(BlueprintReadOnly,Category = "Spirit Attribute Set", ReplicatedUsing= OnRep_FlatSpirit)
	FGameplayAttributeData FlatSpirit;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, FlatSpirit)

	

	UPROPERTY(BlueprintReadOnly,Category = "Spirit Attribute Set", ReplicatedUsing= OnRep_SpiritResistance)
	FGameplayAttributeData SpiritResistance;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, SpiritResistance)

	UPROPERTY(BlueprintReadOnly,Category = "Spirit Attribute Set", ReplicatedUsing= OnRep_SpiritResistanceReduction)
	FGameplayAttributeData SpiritResistanceReduction;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, SpiritResistanceReduction)

	UPROPERTY(BlueprintReadOnly,Category = "Spirit Attribute Set", ReplicatedUsing= OnRep_SpiritLifeSteal)
	FGameplayAttributeData SpiritLifeSteal;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, SpiritLifeSteal)

	UPROPERTY(BlueprintReadOnly,Category = "Melee Attribute Set", ReplicatedUsing= OnRep_MeleeDamage)
	FGameplayAttributeData MeleeDamage;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MeleeDamage)
	
	UPROPERTY(BlueprintReadOnly,Category = "Melee Attribute Set", ReplicatedUsing= OnRep_MeleeResistance)
	FGameplayAttributeData MeleeResistance;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MeleeResistance)

	UPROPERTY(BlueprintReadOnly,Category = "Melee Attribute Set", ReplicatedUsing= OnRep_MeleeResistanceReduction)
	FGameplayAttributeData MeleeResistanceReduction;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MeleeResistanceReduction)

	UPROPERTY(BlueprintReadOnly,Category = "Melee Attribute Set", ReplicatedUsing= OnRep_MeleeLifeSteal)
	FGameplayAttributeData MeleeLifeSteal;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MeleeLifeSteal)

	// Percent bonus applied to all outgoing damage (spirit, weapon and melee), on top of the type-specific bonuses.
	UPROPERTY(BlueprintReadOnly,Category = "Damage Attribute Set", ReplicatedUsing= OnRep_UniversalDamage)
	FGameplayAttributeData UniversalDamage;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, UniversalDamage)


	UPROPERTY(BlueprintReadOnly,Category="CooldownAndDuration",ReplicatedUsing= OnRep_CooldownReduction)
	FGameplayAttributeData CooldownReduction;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, CooldownReduction)

	UPROPERTY(BlueprintReadOnly,Category="CooldownAndDuration",ReplicatedUsing= OnRep_DebufReduction)
	FGameplayAttributeData DebufReduction;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, DebufReduction)

	UPROPERTY(BlueprintReadOnly,Category="CooldownAndDuration",ReplicatedUsing= OnRep_DurationExtension)
	FGameplayAttributeData DurationExtension;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, DurationExtension)

	

	//AbilityCharges
	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_AbilityCharges_1)
	FGameplayAttributeData AbilityCharges_1;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, AbilityCharges_1)

	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_MaxCharges_1)
	FGameplayAttributeData MaxCharges_1;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MaxCharges_1)

	// --- SLOT 2 ---
	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_AbilityCharges_2)
	FGameplayAttributeData AbilityCharges_2;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, AbilityCharges_2)

	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_MaxCharges_2)
	FGameplayAttributeData MaxCharges_2;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MaxCharges_2)

	// --- SLOT 3 ---
	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_AbilityCharges_3)
	FGameplayAttributeData AbilityCharges_3;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, AbilityCharges_3)

	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_MaxCharges_3)
	FGameplayAttributeData MaxCharges_3;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MaxCharges_3)

	// --- SLOT 4 ---
	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_AbilityCharges_4)
	FGameplayAttributeData AbilityCharges_4;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, AbilityCharges_4)

	UPROPERTY(BlueprintReadOnly, Category = "Charges", ReplicatedUsing = OnRep_MaxCharges_4)
	FGameplayAttributeData MaxCharges_4;
	ATTRIBUTE_ACCESSORS(UBaseCharAttributeSet, MaxCharges_4)

protected:
	UFUNCTION() virtual void OnRep_AbilityCharges_1(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_MaxCharges_1(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_AbilityCharges_2(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_MaxCharges_2(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_AbilityCharges_3(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_MaxCharges_3(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_AbilityCharges_4(const FGameplayAttributeData& OldValue);
	UFUNCTION() virtual void OnRep_MaxCharges_4(const FGameplayAttributeData& OldValue);
	
	



	void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty) const;

	UFUNCTION()
		virtual void OnRep_CurrentHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
		virtual void OnRep_CurrentSpeed(const FGameplayAttributeData& OldValue);


	UFUNCTION()
		virtual void OnRep_CurrentJump(const FGameplayAttributeData& OldValue);

	UFUNCTION()
		virtual void OnRep_MaximumHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
		virtual void OnRep_HealthRegeneration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_HealingReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_HealingBonus(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_Souls(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_FireRate(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_WeaponDamage(const FGameplayAttributeData& OldValue);

	
	UFUNCTION()
	virtual void OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue);

	
	UFUNCTION()
	virtual void OnRep_MaxAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_WeaponResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_FlatWeapon(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_WeaponLifeSteal(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_WeaponResistanceReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_SpiritDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_SpiritResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_SpiritResistanceReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_FlatSpirit(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_SpiritLifeSteal(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MeleeDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MeleeResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MeleeLifeSteal(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MeleeResistanceReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_UniversalDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_CooldownReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_DebufReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_DurationExtension(const FGameplayAttributeData& OldValue);
	
};


