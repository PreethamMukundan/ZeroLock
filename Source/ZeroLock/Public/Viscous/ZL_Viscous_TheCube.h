// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseConeTargetAbility.h"
#include "ZL_Viscous_TheCube.generated.h"

class AZL_Viscous_Cube;

/**
 * Viscous's The Cube. Cone targeting (UZL_BaseConeTargetAbility) on allies, self-cast by double tap or by confirming
 * with nobody in the cone. The server encases the target in an AZL_Viscous_Cube for Duration: untouchable, healing
 * HealPerSecond, no abilities or own movement, sliding like a block (Cubed mover mode), pushable by anyone's melee and
 * Puddle Punch, blocks other players. Jump breaks it open early. Ends at confirm (the cube lives on its own).
 */
UCLASS()
class ZEROLOCK_API UZL_Viscous_TheCube : public UZL_BaseConeTargetAbility
{
	GENERATED_BODY()

public:
	UZL_Viscous_TheCube();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Cube")
	TSubclassOf<AZL_Viscous_Cube> CubeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Cube")
	FScalableFloat Duration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Cube")
	FScalableFloat HealPerSecond = 50.0f;

	/** Not already in a cube. */
	virtual bool IsTargetAllowed(const AZeroLockCharacter* Target) const override;
	virtual void ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data) override;
};
