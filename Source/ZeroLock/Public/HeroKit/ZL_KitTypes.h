// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/Zero_Item_data.h"
#include "HeroKit/ZL_KitCue.h"
#include "ZL_KitTypes.generated.h"

class AZeroLockCharacter;
class UAbilitySystemComponent;

/**
 * Hero kit: data-driven building blocks for hero abilities (Docs/Claude/HeroKit.md). One FZL_KitPayload says what a hit
 * does to one hero; the kit abilities (area, projectile, ground, target, dash, passive) decide who gets it.
 */

UENUM(BlueprintType)
enum class EZL_KitDamageType : uint8
{
	Spirit,
	Weapon,
	Melee
};

/** Who a kit hit picks. "Self" is the caster. */
UENUM(BlueprintType)
enum class EZL_KitTeam : uint8
{
	Enemies,
	/** Teammates and the caster. */
	Allies,
	/** Teammates, not the caster. */
	AlliesNotSelf,
	/** Everyone but the caster. */
	Everyone
};

/** Shape of an area hit, from its origin along the aim. */
UENUM(BlueprintType)
enum class EZL_KitShape : uint8
{
	/** Range = radius. */
	Sphere,
	/** Range = length, Angle = half angle (flat). */
	Cone,
	/** Range = length, Angle = half width in cm. */
	Line
};

/** What one hit does to one hero. Zero/empty fields are off. Damage, heal, barrier and DoT scale with the ability's PowerScale. */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_KitPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Damage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	EZL_KitDamageType DamageType = EZL_KitDamageType::Spirit;

	/** Kills an enemy at or below this percent of max health (after the damage). 100 = always. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float ExecuteBelowHealthPercent = 0.f;

	/** Spirit damage per second over DotDuration (burn, bleed, poison). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DotPerSecond = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DotDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	float Heal = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	float HealMaxHealthPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	float Barrier = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	float BarrierDuration = 0.f;

	/** Removes the target's combat barrier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	bool bStripBarrier = false;

	/** Removes the target's combat barrier and gives it to the caster for StolenBarrierDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	bool bStealBarrier = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing", meta = (EditCondition = "bStealBarrier"))
	float StolenBarrierDuration = 8.f;

	/** Removes enemy timed effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	bool bCleanse = false;

	/** Stat changes for StatDuration (negative = debuff; MoveSlowPercent -30 = 30% slow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	TArray<FZL_ItemStatValue> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float StatDuration = 0.f;

	/** Stun, Root, Silence, Stealth, Invulnerable... for StatusDuration. Stun and Root also show the victim's status bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	TArray<EZL_ItemStatus> Statuses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	float StatusDuration = 0.f;

	/** Any other tags, for TagDuration (e.g. Zerolock.Status.Immortal, Zerolock.Cheat.NoCooldowns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	FGameplayTagContainer TimedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	float TagDuration = 0.f;

	/** Pushes the target away from the hit's origin (flat), cm/s. Negative pulls it toward the origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float KnockbackSpeed = 0.f;

	/** Upward speed added to the knockback, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float KnockUpSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float KnockbackDuration = 0.2f;

	/** Drags the target to the hit's origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bPullToOrigin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (EditCondition = "bPullToOrigin"))
	float PullDuration = 0.25f;

	/** Seconds added to the target's running ability cooldowns (negative shortens them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldowns")
	float CooldownDelay = 0.f;

	/** Gameplay cue on the target when this lands (Duration > 0: a loop that follows them: freezes, buffs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	FZL_KitCue HitCue;

	bool IsEmpty() const;
};

/** An area hit: shape, size and who it picks. */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_KitArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EZL_KitShape Shape = EZL_KitShape::Sphere;

	/** Radius (Sphere) or length (Cone, Line) in cm. Scaled by the hero's ability range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Range = 400.f;

	/** Half angle in degrees (Cone) or half width in cm (Line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Angle = 45.f;

	/** Heroes behind a wall (WorldStatic) aren't hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequireLineOfSight = true;
};

namespace ZLKit
{
	/** Whether Other is on the Team side of Hero. Dead heroes never are. */
	ZEROLOCK_API bool MatchesTeam(AZeroLockCharacter* Hero, AZeroLockCharacter* Other, EZL_KitTeam Team);

	/** Every live hero of Team in the area. Direction is flattened for Cone and Line. */
	ZEROLOCK_API void FindHeroes(AZeroLockCharacter* Hero, const FVector& Origin, const FVector& Direction, const FZL_KitArea& Area, float RangeScale,
		EZL_KitTeam Team, TArray<AZeroLockCharacter*>& OutHeroes);

	/** Server. Applies Payload to Target. Origin is where the hit came from (knockback and pull). Source is the damage
	 *  source for telemetry and the effect context (pass the ability or the projectile). */
	ZEROLOCK_API void ApplyPayload(const UObject* Source, AZeroLockCharacter* Hero, AZeroLockCharacter* Target, const FVector& Origin,
		const FZL_KitPayload& Payload, float Scale = 1.f);

	/** Server. Tags for Duration through a timed effect from Source. */
	ZEROLOCK_API void ApplyTimedTags(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, const FGameplayTagContainer& Tags, float Duration);

	/** Debug draw of an area on every PIE window (development builds). */
	ZEROLOCK_API void DrawArea(const UWorld* World, const FVector& Origin, const FVector& Direction, const FZL_KitArea& Area, float RangeScale, const FColor& Color);
}
