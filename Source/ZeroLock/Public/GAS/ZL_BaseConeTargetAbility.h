// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/Base_GA_TargetActors.h"
#include "GAS/TargetActors/ZL_GATA_ConeSingleTarget.h"
#include "ZL_BaseConeTargetAbility.generated.h"

class AZeroLockCharacter;

/**
 * Base for abilities that pick one hero in a cone (AZL_GATA_ConeSingleTarget, Confirm style). Press: aim, the target
 * gets a marker; left click (Confirm) casts on them; Cancel or re-press backs out for free.
 * TargetTeam picks enemies, allies or both. Self-cast: bSelfCastOnDoubleTap (re-press within SelfCastWindow) and/or
 * bSelfCastWithoutTarget (confirm with nobody in the cone).
 * Before the commit (client and server) CanConfirmTarget re-checks the cone with ServerRangeSlack/ServerAngleSlack,
 * the subclass's IsTargetAllowed, and on the server that an enemy target isn't Untouchable.
 * Subclasses override ConeTargetConfirmed (default: end the ability) and IsTargetAllowed.
 */
UCLASS(Abstract)
class ZEROLOCK_API UZL_BaseConeTargetAbility : public UBase_GA_TargetActors
{
	GENERATED_BODY()

public:
	UZL_BaseConeTargetAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnTargetActorSpawned(AGameplayAbilityTargetActor_Trace* TargetActor) override;
	virtual bool CanConfirmTarget(const FGameplayAbilityTargetDataHandle& Data) override;
	virtual void AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data) override;

protected:
	/** Cone length from the hero's view point. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	FScalableFloat Range = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = 0, ClampMax = 90))
	float ConeHalfAngle = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	EZL_ConeTargetTeam TargetTeam = EZL_ConeTargetTeam::Enemies;

	/** Pressing the ability again within SelfCastWindow casts it on yourself (a later re-press still cancels). */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Self Cast")
	bool bSelfCastOnDoubleTap = false;

	/** Seconds after the first press in which a re-press counts as a double tap. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Self Cast", meta = (EditCondition = "bSelfCastOnDoubleTap", ClampMin = 0))
	float SelfCastWindow = 0.3f;

	/** Confirming with nobody in the cone casts on yourself instead of keeping aiming. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Self Cast")
	bool bSelfCastWithoutTarget = false;

	/** Server re-check tolerance: the target moved a bit since the client confirmed. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float ServerRangeSlack = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float ServerAngleSlack = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	bool bDrawTargetingDebug = false;

	/** Double tap = self-cast, otherwise cancel. */
	virtual void OnTargetRepressed(float TimeWaited) override;

	/** Extra per-target rule on top of the cone (client aim and the confirm re-check). Also asked for self-casts. */
	virtual bool IsTargetAllowed(const AZeroLockCharacter* Target) const { return true; }

	/** The target passed the checks and the ability is committed. Target may be the caster (self-cast). Default ends the ability. */
	virtual void ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data);

	bool AllowsSelfCast() const { return bSelfCastOnDoubleTap || bSelfCastWithoutTarget; }

	AZeroLockCharacter* GetTargetFromData(const FGameplayAbilityTargetDataHandle& Data) const;

private:
	TWeakObjectPtr<AZL_GATA_ConeSingleTarget> ConeActor;
};
