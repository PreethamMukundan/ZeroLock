// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/Base_GA_TargetActors.h"
#include "ZL_Doorman_HotelGuest.generated.h"

class AZeroLockCharacter;
class AZL_Doorman_Hotel;
class AZL_Doorman_HotelPlaceholder;
class UAbilityTask_WaitInputPress;
class UGameplayEffect;
enum class EZL_HotelCheckout : uint8;

/**
 * Doorman's ultimate. Press: AZL_GATA_ConeSingleTarget marks one enemy in a small cone in front of him; left click
 * (Confirm) sends them, Cancel or re-press backs out for free. The server re-checks the target (with some slack), then
 * AZL_Doorman_Hotel::CheckIn takes the target's body to the hotel: placeholder left in the map, EntryDamage (spirit),
 * HUD timer, movement abilities blocked. Reaching the exit elevator sends them back; running out of TimeLimit costs
 * LateCheckoutDamage and sends them back; dying sends them back before the death logic.
 * The ability stays active until the guest is out (cooldown starts at confirm). The stay is the hotel's: if Doorman
 * dies or the ability is cancelled, the guest's stay carries on.
 */
UCLASS()
class ZEROLOCK_API UZL_Doorman_HotelGuest : public UBase_GA_TargetActors
{
	GENERATED_BODY()

public:
	UZL_Doorman_HotelGuest();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnTargetActorSpawned(AGameplayAbilityTargetActor_Trace* TargetActor) override;
	virtual bool CanConfirmTarget(const FGameplayAbilityTargetDataHandle& Data) override;
	virtual void AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data) override;

protected:
	/** Cone length from Doorman's view point. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	FScalableFloat Range = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float ConeHalfAngle = 12.0f;

	/** Server re-check tolerance: the target moved a bit since the client confirmed. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float ServerRangeSlack = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float ServerAngleSlack = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	bool bDrawTargetingDebug = false;

	/** Seconds the guest has to reach the exit elevator. */
	UPROPERTY(EditDefaultsOnly, Category = "Hotel")
	FScalableFloat TimeLimit = 8.0f;

	/** Spirit damage when the guest checks in. */
	UPROPERTY(EditDefaultsOnly, Category = "Hotel")
	FScalableFloat EntryDamage = 100.0f;

	/** Extra spirit damage when the time limit runs out. */
	UPROPERTY(EditDefaultsOnly, Category = "Hotel")
	FScalableFloat LateCheckoutDamage = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Hotel")
	TSubclassOf<UGameplayEffect> GuestEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Hotel")
	TSubclassOf<AZL_Doorman_HotelPlaceholder> PlaceholderClass;

	/** Spawned at HotelSpawnLocation only if the map has no hotel placed. */
	UPROPERTY(EditDefaultsOnly, Category = "Hotel")
	TSubclassOf<AZL_Doorman_Hotel> HotelClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hotel")
	FVector HotelSpawnLocation = FVector(0.0f, 0.0f, 30000.0f);

	TWeakObjectPtr<AZeroLockCharacter> GuestRef;
	TWeakObjectPtr<AZL_Doorman_Hotel> HotelRef;
	FDelegateHandle StayEndedHandle;
	/** Past targeting: re-press no longer cancels. */
	bool bTargetConfirmed = false;
	bool bHasEnded = false;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> RepressTask;

	AZeroLockCharacter* GetTargetFromData(const FGameplayAbilityTargetDataHandle& Data) const;
	bool IsTargetAllowed(const AZeroLockCharacter* Target) const;

	UFUNCTION()
	void OnRepressed(float TimeWaited);

	void OnStayEnded(AZeroLockCharacter* Guest, EZL_HotelCheckout Reason);
};
