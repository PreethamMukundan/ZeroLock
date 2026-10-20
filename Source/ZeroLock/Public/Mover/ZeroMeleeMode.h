// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "MoverSimulationTypes.h"
#include "ZeroMeleeMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeHitMoverDelegate);
/** HitActor: what the lunge ran into, or null when it ran out of time. StartLocation: where the lunge began. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMeleeLungeEnded, AActor* /*HitActor*/, const FVector& /*StartLocation*/);

USTRUCT(BlueprintType)
struct FZeroMeleeSyncState : public FMoverDataStructBase
{
	GENERATED_BODY()

	// Tracks exactly how long the character has been lunging across network rollbacks
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	float ElapsedTimeMs = 0.0f;

	/** Where the current lunge began (set on entry); the strike sweeps the whole path from here. */
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FVector StartLocation = FVector::ZeroVector;

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual FMoverDataStructBase* Clone() const override { return new FZeroMeleeSyncState(*this); }
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override
	{
		Ar << ElapsedTimeMs;
		Ar << StartLocation;
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FZeroMeleeSyncState> : public TStructOpsTypeTraitsBase2<FZeroMeleeSyncState>
{
	enum { WithNetSerializer = true,WithCopy = true };
};


UCLASS()
class ZEROLOCK_API UZeroMeleeMode : public UBaseMovementMode
{
	GENERATED_BODY()
    
public:
	UZeroMeleeMode();
    
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	/** Restarts the lunge timer on every entry (it used to carry over, so a second lunge ended after one tick). */
	virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

	UPROPERTY(BlueprintAssignable, Category = "Mover|Melee")
	FOnMeleeHitMoverDelegate MeleeHitDelegate;

	/** Fired once when a lunge ends (enemy hit or MaxDuration), never while resimulating. The pawn deals the strike
	 * from it, server only (AZeroLockCharacter::HandleMeleeLungeEnded). */
	FOnMeleeLungeEnded OnMeleeLungeEnded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Melee")
	float LungeSpeed = 1500.0f;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Melee")
	float MaxDuration = 0.25f;

	/** The enemy sweep looks this far past the step and the lunge stops this short of the enemy, so a target standing
	 * right next to you stops the lunge instead of being slid past. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Melee", meta = (ClampMin = "0"))
	float HitReach = 40.0f;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Melee")
	TEnumAsByte<ECollisionChannel> EnemyTraceChannel = ECC_GameTraceChannel1;
};