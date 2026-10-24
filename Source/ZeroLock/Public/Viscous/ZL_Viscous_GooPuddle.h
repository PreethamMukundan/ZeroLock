// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ZL_Viscous_GooPuddle.generated.h"

class AZeroLockCharacter;
class UGameplayEffect;
class USphereComponent;
class UStaticMeshComponent;

/**
 * A puddle of Viscous's goo (left by AZL_Projectile_Viscous_Splatter). Replicated, lives PuddleDuration. Server:
 * SlowArea (sphere of PuddleRadius, overlaps the Hero object type) slows enemies of its instigator while they're
 * inside: SlowEffect (UZL_GE_MoveSlow, MoveSpeedMultiplier = SlowMultiplier, + Zerolock.Status.Slowed) on enter,
 * removed on leave and when the puddle goes away. Allies aren't affected.
 */
UCLASS()
class ZEROLOCK_API AZL_Viscous_GooPuddle : public AActor
{
	GENERATED_BODY()

public:
	AZL_Viscous_GooPuddle();

	/** Server, before FinishSpawning. */
	void InitPuddle(float InRadius, float InDuration, float InSlowMultiplier);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Puddle")
	TObjectPtr<USphereComponent> SlowArea;

	/** Placeholder look: flat engine Cylinder sized to the radius, tinted GooColor. */
	UPROPERTY(VisibleAnywhere, Category = "Puddle")
	TObjectPtr<UStaticMeshComponent> PuddleMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Puddle")
	FLinearColor GooColor = FLinearColor(0.2f, 0.85f, 0.25f);

	UPROPERTY(EditDefaultsOnly, Category = "Puddle")
	float PuddleThickness = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Puddle")
	TSubclassOf<UGameplayEffect> SlowEffect;

	UPROPERTY(ReplicatedUsing = OnRep_PuddleRadius)
	float PuddleRadius = 250.0f;

	float PuddleDuration = 4.0f;
	float SlowMultiplier = 0.6f;

	/** Enemies inside and the slow each one got from this puddle. */
	TMap<TWeakObjectPtr<AZeroLockCharacter>, FActiveGameplayEffectHandle> SlowedVillans;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_PuddleRadius();

	void ApplyRadius();

	UFUNCTION()
	void OnSlowAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSlowAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void SlowVillan(AZeroLockCharacter* Villan);
	void ReleaseVillan(AZeroLockCharacter* Villan);
};
