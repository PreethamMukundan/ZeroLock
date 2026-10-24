// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Viscous_GooBall.generated.h"

class AZeroLockCharacter;
class UGameplayEffect;
class UStaticMeshComponent;

/**
 * The look and the impacts of Viscous's Goo Ball. Replicated, attached to Viscous (Hero, RepNotify) on every machine:
 * hides his mesh, shows BallMesh (engine Sphere sized to BallRadius) and spins it with his velocity (cosmetic).
 * Server: every tick, enemies within BallRadius + ImpactReach while he moves faster than MinImpactSpeed take
 * ImpactDamage (spirit) and StunEffect, again after RehitDelay (heroes don't collide, so the ball rolls through them);
 * parrying or Untouchable enemies are skipped. The movement itself is the GooBall mover mode (UZeroGooBallMode).
 */
UCLASS()
class ZEROLOCK_API AZL_Viscous_GooBall : public AActor
{
	GENERATED_BODY()

public:
	AZL_Viscous_GooBall();

	/** Server, right after spawning. */
	void StartBall(AZeroLockCharacter* InHero, float InImpactDamage, TSubclassOf<UGameplayEffect> InStunEffect, float InStunDuration);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Goo Ball")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Goo Ball")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	/** Visual radius (cm). Keep it equal to the mover's GooBallRadius (the collision). */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	float BallRadius = 90.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	FLinearColor BallColor = FLinearColor(0.25f, 0.9f, 0.35f);

	/** Extra reach past the ball's surface for hitting enemies (their capsules are inside it when it rolls through). */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	float ImpactReach = 20.0f;

	/** No impacts while slower than this (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball")
	float MinImpactSpeed = 300.0f;

	/** Seconds before the same enemy can be hit again. */
	UPROPERTY(EditDefaultsOnly, Category = "Goo Ball", meta = (ClampMin = 0))
	float RehitDelay = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Hero)
	TObjectPtr<AZeroLockCharacter> Hero;

	float ImpactDamage = 0.0f;
	float StunDuration = 0.0f;
	TSubclassOf<UGameplayEffect> StunEffect;
	/** Enemy → world time of its last hit (RehitDelay). */
	TMap<TWeakObjectPtr<AZeroLockCharacter>, double> LastHitTimes;
	FQuat SpinRotation = FQuat::Identity;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_Hero();

	/** Every machine: ride on the hero and hide his mesh. */
	void AttachToHero();

	void CheckImpacts();
};
