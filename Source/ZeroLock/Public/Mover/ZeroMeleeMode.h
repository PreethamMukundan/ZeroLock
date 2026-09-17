// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "MoverSimulationTypes.h"
#include "ZeroMeleeMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeHitMoverDelegate);

USTRUCT(BlueprintType)
struct FZeroMeleeSyncState : public FMoverDataStructBase
{
	GENERATED_BODY()

	// Tracks exactly how long the character has been lunging across network rollbacks
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	float ElapsedTimeMs = 0.0f;
	
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual FMoverDataStructBase* Clone() const override { return new FZeroMeleeSyncState(*this); }
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override 
	{ 
		Ar << ElapsedTimeMs; 
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
	
	UPROPERTY(BlueprintAssignable, Category = "Mover|Melee")
	FOnMeleeHitMoverDelegate MeleeHitDelegate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Melee")
	float LungeSpeed = 1500.0f;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Melee")
	float MaxDuration = 0.25f;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Melee")
	TEnumAsByte<ECollisionChannel> EnemyTraceChannel = ECC_GameTraceChannel1;
};