// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Items/Zero_Item_data.h"
#include "ZL_ItemProcComponent.generated.h"

class UAbilitySystemComponent;
class UBaseCharAbilitySystemComponent;
class AZeroLockCharacter;

/**
 * Runs the Procs of every owned item: the hero's passive item effects, as data (FZL_ItemProc), so items need no ability
 * asset. Server only: the damage helpers on UBaseCharAbilitySystemComponent feed it events, a timer drives the rest.
 */
UCLASS(ClassGroup = (Custom))
class ZEROLOCK_API UZL_ItemProcComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZL_ItemProcComponent();

	void AddItem(const UZero_Item_data* Item);
	void RemoveItem(const UZero_Item_data* Item);

	/** A respawn strips every effect, so what the procs were holding is gone. */
	void ResetAfterRespawn();

	/** Multiplier for a hit about to land, from procs with a BonusDamagePercent. Starts their cooldowns. */
	float GetDamageScale(EZL_ProcTrigger Trigger, AZeroLockCharacter* Target);

	/**
	 * Other is whoever the event involves: the target of a hit, the attacker of a taken one, null for a cast.
	 * With an ImbueInput, only items imbued on that ability react.
	 */
	void NotifyEvent(EZL_ProcTrigger Trigger, AZeroLockCharacter* Other, float Damage, int32 ImbueInput = 0);

	/** Spirit damage over time, from something other than a proc. */
	void AddDot(AZeroLockCharacter* Target, float PerSecond, float MaxHealthPercent, float Duration);

	/** An area hurting enemies until it runs out. */
	void AddZone(const FVector& Location, float Radius, float Duration, float PerSecond);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FTargetState
	{
		FActiveGameplayEffectHandle Handle;
		int32 Stacks = 0;
		int32 BuildUp = 0;
	};

	struct FDamageEntry
	{
		double Time = 0.0;
		float Damage = 0.f;
	};

	struct FRunningProc
	{
		const UZero_Item_data* Item = nullptr;
		const FZL_ItemProc* Proc = nullptr;
		double ReadyTime = 0.0;
		TArray<FDamageEntry> DamageLog;
		TMap<TWeakObjectPtr<UAbilitySystemComponent>, FTargetState> Targets;
		FActiveGameplayEffectHandle SelfHandle;
		int32 SelfStacks = 0;
	};

	struct FRunningDot
	{
		const FZL_ItemProc* Proc = nullptr;
		TWeakObjectPtr<AZeroLockCharacter> Target;
		float PerSecond = 0.f;
		float MaxHealthPercent = 0.f;
		double EndTime = 0.0;
	};

	struct FRunningZone
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.f;
		float PerSecond = 0.f;
		double EndTime = 0.0;
	};

	AZeroLockCharacter* GetHero() const;
	double GetTime() const;
	float GetHealthPercent(const AZeroLockCharacter* Hero) const;

	/** Cooldown, health, range and chance. With a Damage event it also counts the damage window and the build up. */
	bool PassesConditions(FRunningProc& Proc, AZeroLockCharacter* Other, float Damage, bool bCountHit);
	void Fire(FRunningProc& Proc, AZeroLockCharacter* Other, float Damage);
	void ApplyToTarget(FRunningProc& Proc, AZeroLockCharacter* Target, float Damage);
	void ApplyToSelf(FRunningProc& Proc, float Damage);

	void Update();
	void UpdateHeld(FRunningProc& Proc);
	void UpdateAura(FRunningProc& Proc);
	void UpdateDots();
	void Release(FRunningProc& Proc);

	TArray<FRunningProc> Running;
	TArray<FRunningDot> Dots;
	TArray<FRunningZone> Zones;
	FTimerHandle UpdateHandle;

	/** A proc's own damage must not trigger procs. */
	bool bFiring = false;
};
