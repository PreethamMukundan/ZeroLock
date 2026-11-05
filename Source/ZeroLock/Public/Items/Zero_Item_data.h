//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Zero_Item_data.generated.h"

class UGameplayEffect;
class UBaseGameplayAbility;
/**
 * 
 */

UENUM(BlueprintType)
enum class EZeroLockItemType : uint8
{
	Weapon UMETA(DisplayName="WeaponItems"),
	Spirit UMETA(DisplayName="SpiritItems"),
	Vitality UMETA(DisplayName="VitalityItems")
	
	
};
UENUM(BlueprintType)
enum class EZeroLockItemTier : uint8
{
	Tier1 UMETA(DisplayName="Tier1"),
	Tier2 UMETA(DisplayName="Tier2"),
	Tier3 UMETA(DisplayName="Tier3"),
	Tier4 UMETA(DisplayName="Tier4"),
	Tier5 UMETA(DisplayName="Tier5")
};

/** What an item's Stats entry changes. UZL_GE_ItemStats maps each one to an attribute. */
UENUM(BlueprintType)
enum class EZL_ItemStat : uint8
{
	MaxHealth,
	WeaponDamage,
	MeleeDamage,
	SpiritPower,
	WeaponResist,
	SpiritResist,
	MeleeResist,
	WeaponLifesteal,
	SpiritLifesteal,
	CooldownReduction,
	HealingBonus,
	MaxStamina,
	AmmoFlat,
	AmmoPercent,
	StaminaRegenPercent,
	FireRatePercent,
	HealthRegen,
	OutOfCombatRegen,
	MoveSpeed,
	SprintSpeed,
	BulletSpeedPercent,
	DebuffResist,
	SlowResist,
	SlideDistance,
	MeleeDistance,
	AirControl,
	AbilityDuration,
	AbilityRange,
	AbilityCharges,
	ChargeCooldownReduction,
	SpiritPowerPercent,
	MoveSlowPercent,
	AllDamagePercent,
	HealingReduction
};

/** How an item's active is cast. None = no active. */
UENUM(BlueprintType)
enum class EZL_ActiveItemTarget : uint8
{
	None,
	/** Instant, centred on the hero. */
	Self,
	/** Aim at an ally with the cone; confirm with nobody in it, or double tap, casts on yourself. */
	Ally,
	/** Aim at an enemy with the cone. */
	Enemy,
	/** Pick a spot on the ground. */
	Ground
};

/** Who an active with a Radius hits around its cast point. */
UENUM(BlueprintType)
enum class EZL_ActiveItemTeam : uint8
{
	Enemies,
	Allies
};

USTRUCT(BlueprintType)
struct FZL_ItemStatValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EZL_ItemStat Stat = EZL_ItemStat::MaxHealth;

	/** Percent stats are whole numbers: 20 = 20%. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.f;
};

/** Timed states items put on heroes (ZLItemStatus). */
UENUM(BlueprintType)
enum class EZL_ItemStatus : uint8
{
	None,
	/** No hero abilities. Items still work. */
	Silence,
	/** No primary fire. */
	Disarm,
	Stun,
	BulletImmune,
	/** Clears crowd control and blocks new crowd control. */
	Unstoppable,
	/** Hidden from enemies until the hero attacks or casts. */
	Stealth,
	/** Takes no damage. */
	Invulnerable,
	/** Visual only: the capsule keeps its size. */
	Enlarged,
	Shrunk
};

/** What sets a proc off. */
UENUM(BlueprintType)
enum class EZL_ProcTrigger : uint8
{
	None,
	BulletHit,
	SpiritHit,
	/** Light or heavy. */
	MeleeHit,
	HeavyMeleeHit,
	BulletTaken,
	SpiritTaken,
	/** Any damage taken from a hero. */
	DamageTaken,
	/** Parried a melee strike. */
	Parry,
	/** Used a hero ability or an item. */
	AbilityCast,
	UltimateCast,
	/** Every Cooldown seconds. */
	Periodic,
	/** While the conditions hold. With a Duration, a burst of that length instead, once per Cooldown. */
	While,
	/** TargetStats on every AreaTeam hero within Radius, for as long as they stay in it. */
	Aura,
	/** A bullet hit in the top of the target's capsule. BulletHit fires as well. */
	Headshot,
	/** Used the ability this item is imbued on. */
	ImbuedCast,
	/** That ability did damage. Only abilities that name themselves as the damage source count, so not their projectiles. */
	ImbuedHit
};

/**
 * A passive item effect as data: one trigger, conditions (all optional), and what to do. UZL_ItemProcComponent runs these,
 * so items need no ability asset. Zero means a field is off. "Target" is whoever the trigger involves: the one hit by a
 * Hit trigger, the attacker for a Taken trigger or Parry.
 */
USTRUCT(BlueprintType)
struct FZL_ItemProc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	EZL_ProcTrigger Trigger = EZL_ProcTrigger::None;

	/** Seconds before it can fire again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float Cooldown = 0.f;

	/** Percent chance. 0 always fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float Chance = 0.f;

	/** Damage needed: in one hit, or in total over DamageWindow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float MinDamage = 0.f;

	/** Seconds that MinDamage is added up over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float DamageWindow = 0.f;

	/** Hits on one target before it fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	int32 BuildUpHits = 0;

	/** Own health percent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float HealthAbove = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float HealthBelow = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float TargetHealthAbove = 0.f;

	/** Distance to the target in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	float MaxRange = 0.f;

	/** Adds to the hit itself, so it does nothing else: anything more is another proc. Only on Hit triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float BonusDamagePercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float SpiritDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float WeaponDamage = 0.f;

	/** Extra spirit damage, as a percent of the target's current health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float TargetHealthPercent = 0.f;

	/** Weapon damage, as a percent of the damage that set this off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float PercentOfHit = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float Heal = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float HealPercentOfDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float HealMaxHealthPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float Barrier = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float BarrierDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<FZL_ItemStatValue> SelfStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<FZL_ItemStatValue> TargetStats;

	/** Seconds SelfStats and TargetStats last. 0 holds them for as long as the While or Aura applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float Duration = 0.f;

	/** Stats multiply by this many stacks at most. A new hit adds one and refreshes the time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	int32 MaxStacks = 0;

	/** Also hits AreaTeam heroes this close to the target, or to the hero with no target. In cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float Radius = 0.f;

	/** At most this many of them, nearest first. 0 is everyone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	int32 MaxTargets = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EZL_ActiveItemTeam AreaTeam = EZL_ActiveItemTeam::Enemies;

	/** Only the heroes around the target, not the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bSkipHitTarget = false;

	/** Spirit damage over time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float DotDamagePerSecond = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float DotMaxHealthPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float DotDuration = 0.f;

	/** On the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EZL_ItemStatus Status = EZL_ItemStatus::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float StatusDuration = 0.f;

	/** Removes enemy timed effects from the hero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bCleanse = false;
};

/** What pressing an item's key does. One generic ability per Target reads this (ZLActiveItem), so items need no ability asset. */
USTRUCT(BlueprintType)
struct FZL_ActiveItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EZL_ActiveItemTarget Target = EZL_ActiveItemTarget::None;

	/** Seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Cooldown = 0.f;

	/** Cast range in cm, for Ally, Enemy and Ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Range = 0.f;

	/** Above 0, hits every AreaTeam hero within this many cm of the cast point instead of just the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EZL_ActiveItemTeam AreaTeam = EZL_ActiveItemTeam::Enemies;

	/** Spirit damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Heal = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Barrier = 0.f;

	/** Seconds the barrier lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BarrierDuration = 0.f;

	/** Stat changes on everyone hit, for Duration seconds. Negative values are debuffs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FZL_ItemStatValue> TimedStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 0.f;

	/** On everyone hit, for StatusDuration seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EZL_ItemStatus> Statuses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StatusDuration = 0.f;

	/** Removes enemy timed effects from everyone hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCleanse = false;

	/** Seconds between the cast and everything above landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Delay = 0.f;

	/** Heal on top of Heal, as a percent of each target's max health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HealMaxHealthPercent = 0.f;

	/** On the caster alone, for Duration seconds, whoever else the item hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FZL_ItemStatValue> CasterStats;

	/** On the caster alone, for StatusDuration seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EZL_ItemStatus> CasterStatuses;

	/** Spirit damage over time on everyone hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DotDamagePerSecond = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DotMaxHealthPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DotDuration = 0.f;

	/** Leaves an area at the cast point, Radius wide, hurting enemies in it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoneDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoneDamagePerSecond = 0.f;

	/** Dashes the caster this many cm the way they face, stopping at walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DashDistance = 0.f;

	/** Dashes the caster to the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDashToTarget = false;

	/** Throws the caster upwards, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LaunchSpeed = 0.f;

	/** Drags everyone hit to the cast point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPullToPoint = false;
};

UCLASS()
class ZEROLOCK_API UZero_Item_data : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SellRefundPercent = 0.5f;

	// Passive stat buffs
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> PassiveEffects;

	/** Stat bonuses while owned, applied through UZL_GE_ItemStats. Needs no effect asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FZL_ItemStatValue> Stats;

	/** The item's key: takes one of the four active slots while owned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FZL_ActiveItem Active;

	/** Buying asks which ability the item goes on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bImbue = false;

	/** Bonuses for that ability alone: CooldownReduction, AbilityRange and AbilityDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FZL_ItemStatValue> ImbueStats;

	/** Passive effects while owned, run by UZL_ItemProcComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FZL_ItemProc> Procs;

	// Active or passive abilities
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSubclassOf<UBaseGameplayAbility>> GrantedAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EZeroLockItemType ItemType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UZero_Item_data*> NextItemsToUpgrade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EZeroLockItemTier ItemTierType;

	bool IsActiveItem() const { return Active.Target != EZL_ActiveItemTarget::None; }

	/** ItemName, or ItemID for items that don't have one. */
	FText GetDisplayName() const
	{
		return ItemName.IsEmpty() ? FText::FromName(ItemID) : FText::FromString(ItemName);
	}
};
