// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseSurfaceTargetAbility.h"
#include "ZL_Solomon_SetupBishop.generated.h"

class AZL_Solomon_Bishop;

/**
 * Solomon's Setup Bishop. While fewer than MaxBishops of his bishops are out, a press places one (free, no cooldown):
 * where he aims (UZL_BaseSurfaceTargetAbility Instant + Ground, up to MaxPlacementRange = Deadlock AbilityCastRange
 * 20 m), on the shared board square there (AZL_Solomon_ChessPiece::FindPlacementSpot) on standable floor. Closer than the bishop's MinPieceSpacing
 * to one of his pieces, or no standable floor = nothing happens. Bishops stay until broken; move them with Move Piece.
 * A press with MaxBishops out breaks all his bishops (server destroys them, squares go with them) and starts the cooldown.
 * CheckCooldown always passes while a bishop is out, so the break works whenever.
 */
UCLASS()
class ZEROLOCK_API UZL_Solomon_SetupBishop : public UZL_BaseSurfaceTargetAbility
{
	GENERATED_BODY()

public:
	UZL_Solomon_SetupBishop();

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Bishop")
	TSubclassOf<AZL_Solomon_Bishop> BishopClass;

	/** Bishops he can have out; a press at this many breaks them all (Deadlock MaxPieces 1, +1 at T2). */
	UPROPERTY(EditDefaultsOnly, Category = "Bishop", meta = (ClampMin = 1))
	int32 MaxBishops = 2;

	/** Board square under the hit, on the floor; false if his pieces hold it. */
	virtual bool GetPlacementTransform(const FHitResult& Hit, FTransform& OutTransform) const override;
	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data) override;

private:
	int32 CountBishops(const AActor* Avatar) const;
	/** Set around the break's commit: the only commit that starts the cooldown. */
	bool bBreaking = false;
};
