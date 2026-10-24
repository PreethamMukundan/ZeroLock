//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/BaseGameplayAbility.h"
#include "Base_GA_TargetActors.generated.h"

class AGATA_SingleLineTrace;
class UAbilityTask_WaitInputPress;
/**
 * 
 */
class AGameplayAbilityTargetActor;
UCLASS()
class ZEROLOCK_API UBase_GA_TargetActors : public UBaseGameplayAbility
{
	GENERATED_BODY()
public:
	UBase_GA_TargetActors();


	UFUNCTION()
	void TargetConfirmed(const FGameplayAbilityTargetDataHandle& Data);
	UFUNCTION()
	void TargetCancelled(const FGameplayAbilityTargetDataHandle& Data);
	UFUNCTION()
	void TargetInputRelease(float TimeHeld);
	/** Actually activate ability, do not call this directly. We'll call it from APAHeroCharacter::ActivateAbilitiesWithTags(). */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	

	virtual void AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data);
	virtual void AbilityCancelledAction(const FGameplayAbilityTargetDataHandle& Data);
	/** Runs on confirm before the commit (client and server): false cancels for free (e.g. the server rejects the target). */
	virtual bool CanConfirmTarget(const FGameplayAbilityTargetDataHandle& Data) { return true; }
	/** Called after the target actor is spawned and set up, before targeting starts. */
	virtual void OnTargetActorSpawned(class AGameplayAbilityTargetActor_Trace* TargetActor) {}

	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ability")
	TSubclassOf<AGameplayAbilityTargetActor> Targetclass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	TSubclassOf<class AGameplayAbilityWorldReticle> ReticleClassX;
	
	class	UAbilityTask_WaitTargetData* WaitTargetTask;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ability")
	EGASTargetConfirmationStyle TargetConfirmationStyle;

	/** Confirm style only: pressing the ability again while aiming calls OnTargetRepressed (default: free cancel). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ability")
	bool bCancelOnRepress = false;

protected:
	/** Set once the target passed CanConfirmTarget: re-presses are ignored from then on. */
	bool bTargetConfirmed = false;

	/** Re-press while aiming, on the aiming player's machine only (a cancel replicates through EndAbility). */
	virtual void OnTargetRepressed(float TimeWaited);

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> RepressTask;

	UFUNCTION()
	void HandleRepress(float TimeWaited);
};
