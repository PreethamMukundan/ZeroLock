// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "ZL_KitCue.generated.h"

class AActor;
class UAbilitySystemComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * Hero kit cosmetics go through gameplay cues: abilities hold a cue tag (GameplayCue.HeroKit.<Hero>.<Ability>.<Role>);
 * the cue notify asset with that tag owns the Niagara system, scale and sound (UZL_GCN_KitBurst / AZL_GCN_KitLoop
 * children, made by Tools/HeroKit/build_heroes.py). Docs/Claude/HeroKit.md "Effects".
 */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_KitCue
{
	GENERATED_BODY()

	/** Empty = no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "GameplayCue"))
	FGameplayTag Cue;

	/** 0: executed once (a burst notify). Above 0: added to the hero it plays on and removed after this many seconds
	 *  (a looping notify that follows them: freezes, buffs, trails). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 0.f;

	bool IsSet() const { return Cue.IsValid(); }
};

namespace ZLKitCue
{
	/** Fills cue parameters: Location, Normal = facing, RawMagnitude = radius (notifies scale with it), Instigator. */
	ZEROLOCK_API FGameplayCueParameters MakeParams(AActor* Instigator, const FVector& Location, const FRotator& Facing, float Radius);

	/**
	 * Plays Cue on the hero that owns ASC. Duration 0: ExecuteGameplayCue (replicated; called inside an ability's prediction
	 * window on the owner it's predicted). Duration > 0 (server): AddGameplayCue now, RemoveGameplayCue after Duration.
	 */
	ZEROLOCK_API void Play(UAbilitySystemComponent* ASC, const FZL_KitCue& Cue, const FVector& Location, const FRotator& Facing, float Radius = 0.f);

	/** This machine only, on any actor (projectiles, zones: every machine simulates them already). Add/remove a loop. */
	ZEROLOCK_API void AddLocal(AActor* Target, const FGameplayTag& Cue, float Radius = 0.f);
	ZEROLOCK_API void RemoveLocal(AActor* Target, const FGameplayTag& Cue);
	ZEROLOCK_API void ExecuteLocal(AActor* Target, const FGameplayTag& Cue, const FVector& Location, const FRotator& Facing, float Radius = 0.f);
}

/** What a hero kit cue notify spawns. Shared by the burst and loop notifies. */
USTRUCT(BlueprintType)
struct ZEROLOCK_API FZL_KitCueVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Scale = 1.f;

	/** Scale also × (the cue's radius (RawMagnitude) / ReferenceRadius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bScaleWithRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bScaleWithRadius"))
	float ReferenceRadius = 300.f;

	/** cm along the cue's facing (Normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ForwardOffset = 0.f;

	/** cm up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float UpOffset = 0.f;

	/** Follows the hero the cue plays on instead of staying at the cue's location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAttachToTarget = false;

	/** Niagara user float parameters set on spawn (name without "User."), e.g. SpikeCount, SpikeHeight, Lifetime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, float> FloatParameters;

	/** Also passes the cue's radius to this Niagara user float (None = don't). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RadiusParameter = TEXT("Radius");

	/** Spawns the system (and plays the sound) for this cue. Returns the component or null. */
	UNiagaraComponent* Spawn(AActor* Target, const FGameplayCueParameters& Parameters) const;
};

/** One-shot hero kit effect: ExecuteGameplayCue with its tag spawns Visual. Make a BP child per effect, set its tag. */
UCLASS(Blueprintable)
class ZEROLOCK_API UZL_GCN_KitBurst : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Kit")
	FZL_KitCueVisual Visual;
};

/** Looping hero kit effect: AddGameplayCue spawns Visual attached to the target, RemoveGameplayCue stops it. */
UCLASS(Blueprintable)
class ZEROLOCK_API AZL_GCN_KitLoop : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AZL_GCN_KitLoop();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Kit")
	FZL_KitCueVisual Visual;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Spawned;
};
