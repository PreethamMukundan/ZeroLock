// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Tempest_Vortex.generated.h"

class AZeroLockCharacter;
class UStaticMeshComponent;

/**
 * Tempest's wind dome (Eye of the Storm). A replicated sphere (engine sphere scaled to Radius, BP material e.g.
 * MI_HeroKit_WindBall with "Color" = DomeColor) that lives Duration seconds. The server pulses every Interval: enemies
 * within Radius (no line of sight) take TickDamage spirit and a SlowPercent slow for SlowDuration.
 * A caster keeps at most MaxPerCaster domes; the oldest goes. Spawn with AZL_Tempest_Vortex::SpawnFor (server).
 */
UCLASS()
class ZEROLOCK_API AZL_Tempest_Vortex : public AActor
{
	GENERATED_BODY()

public:
	AZL_Tempest_Vortex();

	/** Server. DamageSource groups the damage for telemetry (the ult ability). */
	static AZL_Tempest_Vortex* SpawnFor(TSubclassOf<AZL_Tempest_Vortex> VortexClass, AZeroLockCharacter* Caster, const FVector& Location, const UObject* DamageSource);

	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Vortex")
	TObjectPtr<UStaticMeshComponent> DomeMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	float Radius = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	float Duration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	float Interval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	float TickDamage = 10.0f;

	/** 40 = 40% slower. */
	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	float SlowPercent = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	float SlowDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	int32 MaxPerCaster = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	FLinearColor DomeColor = FLinearColor(2.2f, 5.0f, 3.5f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Vortex")
	bool bDrawDebug = false;

private:
	void Pulse();

	FTimerHandle PulseTimer;
	double SpawnTime = 0.0;
	TWeakObjectPtr<const UObject> DamageSource;
};
