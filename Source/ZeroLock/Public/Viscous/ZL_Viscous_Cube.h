// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ZL_Viscous_Cube.generated.h"

class AZeroLockCharacter;
class UBoxComponent;
class UGameplayEffect;
class UStaticMeshComponent;

/**
 * Viscous's Cube around one hero. Replicated; attached to the hero (CubedHero, RepNotify) on every machine, so it
 * moves with the hero's Mover, which runs the Cubed mode while the hero has Zerolock.Status.Cubed.
 * - Blocker (box of CubeExtent, object type Pawn, blocks Pawn and Hero) stops other players walking through it; the
 *   hero's own capsule ignores this actor. Melee strikes that hit it push its hero (AZeroLockCharacter::PerformMeleeStrike).
 * - Server: CubeEffect (Zerolock.Untouchable + Zerolock.Status.Cubed) and a HUD status timer for Duration, healing
 *   HealPerSecond (every HealInterval, through ApplyHeal). Ends on timeout, the hero's jump (Server_RequestJumpEscape)
 *   or the hero's death: effect removed, timer cleared, destroyed. Carries on if Viscous dies.
 */
UCLASS()
class ZEROLOCK_API AZL_Viscous_Cube : public AActor
{
	GENERATED_BODY()

public:
	AZL_Viscous_Cube();

	/** Server, right after spawning. Healer = Viscous (may be the hero itself). */
	void StartCube(AZeroLockCharacter* InHealer, AZeroLockCharacter* InHero, float InDuration, float InHealPerSecond);

	/** Server. */
	void EndCube();

	AZeroLockCharacter* GetCubedHero() const { return CubedHero; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Cube")
	TObjectPtr<UBoxComponent> Blocker;

	/** Placeholder look: engine Cube sized to CubeExtent, tinted CubeColor. */
	UPROPERTY(VisibleAnywhere, Category = "Cube")
	TObjectPtr<UStaticMeshComponent> CubeMesh;

	/** Half size of the cube (cm). A bit bigger than the hero's capsule (34 × 88). */
	UPROPERTY(EditDefaultsOnly, Category = "Cube")
	FVector CubeExtent = FVector(55.0f, 55.0f, 95.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Cube")
	FLinearColor CubeColor = FLinearColor(0.25f, 0.9f, 0.35f);

	UPROPERTY(EditDefaultsOnly, Category = "Cube")
	TSubclassOf<UGameplayEffect> CubeEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Cube")
	float HealInterval = 0.25f;

	UPROPERTY(ReplicatedUsing = OnRep_CubedHero)
	TObjectPtr<AZeroLockCharacter> CubedHero;

	TWeakObjectPtr<AZeroLockCharacter> Healer;
	float HealPerSecond = 0.0f;
	FActiveGameplayEffectHandle EffectHandle;
	FTimerHandle HealTimer;
	FTimerHandle EndTimer;
	FDelegateHandle EscapeHandle;
	FDelegateHandle PreDeathHandle;
	bool bEnded = false;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_CubedHero();

	/** Every machine: ride on the hero, and the hero's own moves ignore the blocker. */
	void AttachToHero();

	/** Server, once: effect removed, timers and status bar cleared, delegates unbound. */
	void CleanUpCube();

	void HealTick();
	void HandleHeroPreDeath(AZeroLockCharacter* DeadHero);
};
