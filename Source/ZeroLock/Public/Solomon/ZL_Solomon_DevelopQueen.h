// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GAS/ZL_BaseSurfaceTargetAbility.h"
#include "ZL_Solomon_DevelopQueen.generated.h"

class AZL_Solomon_Queen;
class UGameplayEffect;

/**
 * Solomon's ultimate Develop Queen (Deadlock ability_chessmaster_queen): "Summon a queen that crashes down, dealing spirit
 * damage in an area. After a delay, the queen releases squares in straight lines that deal spirit damage again and apply
 * stun. Move (alt fire): the queen leaps to the target location, dealing the same effects again. Re-activate to recall
 * the queen early."
 * Ground target, Confirm style (left click; preview = the crash area), up to MaxPlacementRange 30 m. On confirm (after the
 * commit, the cooldown starts) the server spawns AZL_Solomon_Queen DropHeight above the spot, which crashes onto it.
 * With his queen out, a press recalls it instead (free: CheckCooldown passes, no commit).
 */
UCLASS()
class ZEROLOCK_API UZL_Solomon_DevelopQueen : public UZL_BaseSurfaceTargetAbility
{
	GENERATED_BODY()

public:
	UZL_Solomon_DevelopQueen();

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	TSubclassOf<AZL_Solomon_Queen> QueenClass;

	/** Stun from the squares; its duration is overridden by StunDuration. Empty = the queen's default (BPGE_Stun). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	TSubclassOf<UGameplayEffect> StunEffect;

	/** Spirit damage of the crash and of each square (Deadlock Damage 120). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	FScalableFloat Damage = 120.0f;

	/** Deadlock StunDuration 1.25. */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	FScalableFloat StunDuration = 1.25f;

	/** Slow on every hit (Deadlock SlowPercent 15, SlowDuration 1.5; +15% at T2). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	FScalableFloat SlowPercent = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	FScalableFloat SlowDuration = 1.5f;

	/** Squares along each line (Deadlock TileSteps 3, +1 at T3). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	FScalableFloat TileSteps = 3.0f;

	/** Leaps with Move Piece (a guess: Deadlock limits moves, +2 at T3, but the base count isn't in its data). */
	UPROPERTY(EditDefaultsOnly, Category = "Queen")
	FScalableFloat Moves = 3.0f;

	virtual bool GetPlacementTransform(const FHitResult& Hit, FTransform& OutTransform) const override;
	virtual void PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data) override;

private:
	AZL_Solomon_Queen* FindQueen(const AActor* Avatar) const;
};
