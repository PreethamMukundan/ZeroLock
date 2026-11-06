// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Xayah/ZL_Xayah_ThrowAbility.h"
#include "ZL_Xayah_Featherstorm.generated.h"

class AZL_Xayah_StormPreview;
class UAbilityTask_WaitConfirmCancel;
class UZL_WaitDelay_Task;

/**
 * Ultimate. Xayah leaps back and up, untouchable, and hangs in the air (Floating) while a circle follows her aim. Confirm
 * (or Cancel, or the end of TargetWindow) throws a cone of blades at it; each one drops a feather, ready for Bladecaller.
 * The cost is paid on the press, so Cancel doesn't refund it. The client's blades carry the aim to the server (spawn data).
 */
UCLASS()
class ZEROLOCK_API UZL_Xayah_Featherstorm : public UZL_Xayah_ThrowAbility
{
	GENERATED_BODY()

public:
	UZL_Xayah_Featherstorm();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Speed away from where she's looking during the leap, cm/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	FScalableFloat LeapBackSpeed = 1500.0f;

	/** Upward speed during the leap, cm/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	FScalableFloat LeapUpSpeed = 1600.0f;

	/** Seconds of the leap. Short and fast feels snappy. She stops when it ends, so the height is LeapUpSpeed × this. */
	UPROPERTY(EditDefaultsOnly, Category = "Leap")
	FScalableFloat LeapDuration = 0.25f;

	/** Seconds from the press to the automatic throw. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	FScalableFloat TargetWindow = 1.25f;

	/** Half-angle of the cone of blades, in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
	float ConeHalfAngle = 12.0f;

	/** How far the aim circle reaches. Keep it near the blade's range (speed × InitialLifeSpan). */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float PreviewRange = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	TSubclassOf<AZL_Xayah_StormPreview> PreviewClass;

	/** Spreads the blades evenly over a disc across the cone. */
	virtual FRotator GetBladeRotation(const FRotator& AimRotation, int32 Index) const override;

	UPROPERTY()
	TObjectPtr<UZL_WaitDelay_Task> TargetTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitConfirmCancel> ConfirmTask;

	UPROPERTY()
	TObjectPtr<AZL_Xayah_StormPreview> Preview;

	bool bThrown = false;

	void DestroyPreview();

	UFUNCTION()
	void ThrowNow();

	/** Stops her dead at the top of the leap: Float cancels gravity, so she'd keep rising at the leap's speed. */
	UFUNCTION()
	void OnLeapFinished();
};
