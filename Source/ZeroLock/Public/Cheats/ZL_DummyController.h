// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ZL_DummyController.generated.h"

/**
 * Server-only controller for a cheat target dummy (UZL_CheatComponent::ServerSpawnDummy): any hero class, with its own
 * bot PlayerState so it has a team and takes damage like a player. Immortal (Zerolock.Cheat.Buddha, never below 1 HP)
 * by default; health refills HealDelay seconds after the last hit. A mortal dummy comes back RespawnDelay seconds after
 * dying, in place. Optionally strafes left and right. Not part of the game mode's teams, kills or telemetry.
 */
UCLASS()
class ZEROLOCK_API AZL_DummyController : public AAIController
{
	GENERATED_BODY()

public:
	AZL_DummyController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	bool bImmortal = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	bool bStrafe = false;

	/** Seconds without damage before health refills. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	float HealDelay = 3.f;

	/** Seconds per strafe direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	float StrafeInterval = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	float RespawnDelay = 3.f;

protected:
	virtual void InitPlayerState() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	void UpdateDummy();
	FTimerHandle UpdateTimer;

	float LastHealth = -1.f;
	double LastDamageTime = 0.0;
	double DeathTime = 0.0;
	double StrafeFlipTime = 0.0;
	float StrafeDirection = 1.f;
};
