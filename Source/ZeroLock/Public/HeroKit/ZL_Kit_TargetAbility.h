// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseConeTargetAbility.h"
#include "HeroKit/ZL_KitTypes.h"
#include "ZL_Kit_TargetAbility.generated.h"

/** How a kit target ability moves the caster or the target. */
UENUM(BlueprintType)
enum class EZL_KitTargetMove : uint8
{
	None,
	/** The caster rushes to the target. */
	DashToTarget,
	/** The caster appears behind the target. */
	TeleportBehind,
	/** The caster and the target trade places. */
	SwapPlaces,
	/** The target is dragged to just in front of the caster. */
	PullTargetToCaster
};

/**
 * Hero kit: aim at one hero with the cone (UZL_BaseConeTargetAbility: TargetTeam, Range, self-cast options), confirm, then
 * on the server: TargetPayload PulseCount times PulseInterval apart, the move, and SelfPayload. Pulses after the first run
 * on the hero's timers (the ability has ended by then).
 */
UCLASS()
class ZEROLOCK_API UZL_Kit_TargetAbility : public UZL_BaseConeTargetAbility
{
	GENERATED_BODY()

protected:
	virtual void ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data) override;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FScalableFloat PowerScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload TargetPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit", meta = (ClampMin = 1))
	int32 PulseCount = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float PulseInterval = 0.5f;

	/** Seconds from the confirm to the first pulse. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float HitDelay = 0.0f;

	/** On the caster at the confirm. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	FZL_KitPayload SelfPayload;

	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	EZL_KitTargetMove Move = EZL_KitTargetMove::None;

	/** Seconds the dash or pull takes. */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float MoveTime = 0.2f;

	/** Cue on the caster at the confirm. (The target's look is TargetPayload.HitCue.) */
	UPROPERTY(EditDefaultsOnly, Category = "Kit|FX")
	FZL_KitCue CastCue;

	/** cm behind the target (TeleportBehind) or in front of the caster (PullTargetToCaster). */
	UPROPERTY(EditDefaultsOnly, Category = "Kit")
	float MoveOffset = 150.0f;

	/** Server: before the first pulse. */
	virtual void OnTargetConfirmedServer(AZeroLockCharacter* Hero, AZeroLockCharacter* Target) {}

	/** Server: payload for one pulse. Override to change it per hit (Kaiju's marks). */
	virtual FZL_KitPayload GetPulsePayload(AZeroLockCharacter* Hero, AZeroLockCharacter* Target, int32 PulseIndex) const { return TargetPayload; }

	void DoPulse(AZeroLockCharacter* Target, int32 PulseIndex);
	void DoMove(AZeroLockCharacter* Hero, AZeroLockCharacter* Target);
};
