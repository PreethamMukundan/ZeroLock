// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_HeroSwapPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Optional: step on it to open the hero select screen (AZL_HUDClass::OpenHeroSelect, same as the H key).
 * If AZero_BaseGameModeBase::bHeroSwapRequiresPad is set, ServerSwapHero is only accepted while the pawn is on a pad.
 */
UCLASS(Blueprintable)
class ZEROLOCK_API AZL_HeroSwapPad : public AActor
{
	GENERATED_BODY()

public:
	AZL_HeroSwapPad();

	/** Server-side check used to validate swap requests. */
	bool IsPawnOnPad(const APawn* Pawn) const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeroSwap")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeroSwap")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	/** A pawn younger than this that starts inside the pad (just swapped/spawned there) doesn't open the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeroSwap")
	float IgnoreNewPawnTime = 1.0f;

	/** Extra distance (cm) outside the box the server still accepts a swap from, to cover latency. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeroSwap")
	float ServerCheckTolerance = 150.f;

private:
	/** Only close the screen on step-off if this pad opened it (not if the player opened it with H). */
	bool bOpenedHeroSelect = false;
};
