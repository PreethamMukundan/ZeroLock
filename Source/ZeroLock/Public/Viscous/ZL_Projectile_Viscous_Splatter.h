// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Projectile.h"
#include "ZL_Projectile_Viscous_Splatter.generated.h"

class AZL_Viscous_GooPuddle;
class UStaticMeshComponent;

/**
 * Viscous's Splatter ball. A predicted projectile with gravity that passes through heroes and bounces off the world
 * (WorldStatic) GooBounces times; the next impact ends it. Every impact (bounces and the last one) deals ImpactDamage
 * spirit damage to enemies within ImpactRadius (WorldStatic line of sight) and, on floor-like surfaces (normal within
 * PuddleFloorAngle of up), leaves an AZL_Viscous_GooPuddle. Damage and puddles are server-only (the authoritative copy);
 * the owning ability sets them with ArmSplatter. GooBounces replicates so every copy bounces the same number of times.
 */
UCLASS()
class ZEROLOCK_API AZL_Projectile_Viscous_Splatter : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Projectile_Viscous_Splatter(const FObjectInitializer& ObjectInitializer);

	/** Every copy (fake and server): how many times it bounces before the impact that ends it. */
	void SetGooBounces(int32 InBounces);

	/** Server copy: impact damage and the puddle it leaves. */
	void ArmSplatter(float InImpactDamage, float InImpactRadius, float InPuddleRadius, float InPuddleDuration, float InSlowMultiplier);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Placeholder look: engine Sphere, tinted GooColor. No collision. */
	UPROPERTY(VisibleAnywhere, Category = "Splatter")
	TObjectPtr<UStaticMeshComponent> GooMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Splatter")
	FLinearColor GooColor = FLinearColor(0.2f, 0.85f, 0.25f);

	UPROPERTY(EditDefaultsOnly, Category = "Splatter")
	TSubclassOf<AZL_Viscous_GooPuddle> PuddleClass;

	/** Impacts on normals within this many degrees of up leave a puddle (walls and ceilings don't). */
	UPROPERTY(EditDefaultsOnly, Category = "Splatter", meta = (ClampMin = 0, ClampMax = 90))
	float PuddleFloorAngle = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Splatter")
	bool bDrawDebug = false;

	UPROPERTY(ReplicatedUsing = OnRep_GooBounces)
	int32 GooBounces = 2;

	float ImpactDamage = 0.0f;
	float ImpactRadius = 200.0f;
	float PuddleRadius = 250.0f;
	float PuddleDuration = 4.0f;
	float SlowMultiplier = 0.6f;

	virtual void BeginPlay() override;
	virtual void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity) override;
	virtual void OnStop(const FHitResult& Hit) override;
	/** Passes through heroes: never detonates on overlap. */
	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override {}

	UFUNCTION()
	void OnRep_GooBounces();

	void ApplyBounceLimit();

	/** Server copy: radius damage + puddle at one impact. */
	void Splat(const FHitResult& Hit);

	bool IsAuthSplatter() const { return HasAuthority() && !bIsFakeProjectile; }
};
