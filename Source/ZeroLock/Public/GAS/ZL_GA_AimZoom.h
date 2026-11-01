// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZL_GA_AimZoom.generated.h"

/**
 * Default alt fire (AZeroLockCharacter::AltFireAbility): hold to zoom, like Deadlock's right click. LocalOnly: it only
 * moves the owning player's camera (AZeroMoverPawn::SetAimZoom), nothing gameplay-side. Ends on input release.
 * Deadlock's hero_weapon_base: m_flZoomFOV_Relative -40, m_flZoomMoveSpeedPercent 1.0 (no slow).
 */
UCLASS()
class ZEROLOCK_API UZL_GA_AimZoom : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UZL_GA_AimZoom();

	/** Added to the hero's FOV in Deadlock's 4:3 degrees (m_flZoomFOV_Relative). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoom")
	float ZoomFOVRelative = -40.0f;

	/** Seconds to blend in and out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoom")
	float ZoomBlendTime = 0.15f;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};
