// ZeroMovementData.h
#pragma once

#include "CoreMinimal.h"
#include "MoverDataModelTypes.h"
#include "Engine/DataAsset.h"
#include "ZeroMovementData.generated.h"

USTRUCT(BlueprintType)
struct FZeroMovementInputs : public FMoverDataStructBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
    FVector MoveInput = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
	FRotator LookDir = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
    bool bWantsToCrouch = false;
    
    UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
    bool bSlideIntentValid = true;
    
    UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
    bool bCustomJumpJustPressed = false;
	
	UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
	bool bWantsToZipline = false;
    
    UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
    bool bJumpHold = false;
    
    UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
    bool bWantsToDash = false;
	
	UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
	bool bWantsToMelee = false;
	
	UPROPERTY(BlueprintReadWrite, Category = "ZeroLock|Movement")
	bool bHasAbilityMove = false;

	UPROPERTY(BlueprintReadWrite, Category = "ZeroLock|Movement")
	FVector AbilityVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "ZeroLock|Movement")
	float AbilityMoveDuration = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, Category = "ZeroLock|Movement")
	bool bHasDynamicAbilityMove = false;

	UPROPERTY(BlueprintReadWrite, Category = "ZeroLock|Movement")
	AActor* DynamicTargetActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "ZeroLock|Movement")
	float DynamicMoveDuration = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, Category="Zero")
	bool bHasPullMove = false;
	
	UPROPERTY(BlueprintReadWrite, Category="Zero") 
	FVector PullTarget = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite, Category="Zero") 
	float PullDuration = 0.f;
	
	UPROPERTY(BlueprintReadWrite, Category="Zero")
	bool bWantsStop = false;
	
	UPROPERTY(BlueprintReadWrite, Category="Zero")
	bool bWantsRelease = false;

	UPROPERTY(BlueprintReadWrite, Category="Zero")
	bool bHasTeleport = false;

	UPROPERTY(BlueprintReadWrite, Category="Zero")
	FVector TeleportTarget = FVector::ZeroVector;

	// Teleport also replaces the velocity (portals redirect momentum).
	UPROPERTY(BlueprintReadWrite, Category="Zero")
	bool bTeleportSetsVelocity = false;

	UPROPERTY(BlueprintReadWrite, Category="Zero")
	FVector TeleportVelocity = FVector::ZeroVector;

	// Sampled from the owner's ZeroLock.Movement.Rooted tag when the input is produced, so client and server root on the same frame.
	UPROPERTY(BlueprintReadWrite, Category="Zero")
	bool bIsRooted = false;

    virtual UScriptStruct* GetScriptStruct() const override { return FZeroMovementInputs::StaticStruct(); }
    virtual FMoverDataStructBase* Clone() const override { return new FZeroMovementInputs(*this); }

    virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
    {
       Super::NetSerialize(Ar, Map, bOutSuccess);
       
       Ar << MoveInput;
    	Ar<<LookDir;
    	
       Ar << bWantsToCrouch;
       Ar << bSlideIntentValid; 
       Ar << bCustomJumpJustPressed; 
       Ar << bJumpHold;
       Ar << bWantsToDash;
       Ar << bWantsToMelee;
    	Ar << bWantsToZipline;
    	
    	Ar<< bHasAbilityMove;
    	Ar << AbilityMoveDuration;
    	Ar << AbilityVelocity;
    	
    	Ar << bHasDynamicAbilityMove;
    	Ar << DynamicMoveDuration;
    	
    	Ar << bHasPullMove;
    	Ar << PullTarget;
    	Ar << PullDuration;
    	Ar << bWantsStop;
    	Ar << bWantsRelease;
    	Ar << bHasTeleport;
    	Ar << TeleportTarget;
    	Ar << bTeleportSetsVelocity;
    	Ar << TeleportVelocity;
    	Ar << bIsRooted;

    	Map->SerializeObject(Ar, AActor::StaticClass(), (UObject*&)DynamicTargetActor);
    
    	

       bOutSuccess = true;
       return true;
    }
};

// Per-frame simulation state that must roll back with the rest of the Mover sync state:
// stamina, air jumps and air dashes used since leaving the ground, time left on the current dash,
// and whether the previous frame was rooted / sliding.
USTRUCT(BlueprintType)
struct FZeroMoverSimState : public FMoverDataStructBase
{
	GENERATED_BODY()

	// Negative until the first sim tick fills it with the owner's max stamina.
	UPROPERTY(BlueprintReadOnly, Category = "Zero|State")
	float Stamina = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Zero|State")
	int32 AirJumpsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Zero|State")
	int32 AirDashesUsed = 0;

	// Jumps are ignored while this is above zero.
	UPROPERTY(BlueprintReadOnly, Category = "Zero|State")
	float DashRemainingMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Zero|State")
	bool bWasRooted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Zero|State")
	bool bWasSliding = false;

	virtual UScriptStruct* GetScriptStruct() const override { return FZeroMoverSimState::StaticStruct(); }
	virtual FMoverDataStructBase* Clone() const override { return new FZeroMoverSimState(*this); }

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Super::NetSerialize(Ar, Map, bOutSuccess);
		Ar << Stamina;
		Ar << AirJumpsUsed;
		Ar << AirDashesUsed;
		Ar << DashRemainingMs;
		Ar << bWasRooted;
		Ar << bWasSliding;
		bOutSuccess = true;
		return true;
	}

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FZeroMoverSimState* AuthState = static_cast<const FZeroMoverSimState*>(&AuthorityState);
		return !FMath::IsNearlyEqual(Stamina, AuthState->Stamina, 0.05f) ||
			AirJumpsUsed != AuthState->AirJumpsUsed ||
			AirDashesUsed != AuthState->AirDashesUsed ||
			!FMath::IsNearlyEqual(DashRemainingMs, AuthState->DashRemainingMs, 1.0f) ||
			bWasRooted != AuthState->bWasRooted ||
			bWasSliding != AuthState->bWasSliding;
	}

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
	{
		const FZeroMoverSimState* FromState = static_cast<const FZeroMoverSimState*>(&From);
		const FZeroMoverSimState* ToState = static_cast<const FZeroMoverSimState*>(&To);
		Stamina = FMath::Lerp(FromState->Stamina, ToState->Stamina, Pct);
		AirJumpsUsed = ToState->AirJumpsUsed;
		AirDashesUsed = ToState->AirDashesUsed;
		DashRemainingMs = ToState->DashRemainingMs;
		bWasRooted = ToState->bWasRooted;
		bWasSliding = ToState->bWasSliding;
	}
};

USTRUCT(BlueprintType)
struct FZeroSlideState : public FMoverDataStructBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Zero|State")
    FVector SlideDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Zero|State")
    float TimeSpentSliding = 0.0f;
    
    virtual UScriptStruct* GetScriptStruct() const override { return FZeroSlideState::StaticStruct(); }
    virtual FMoverDataStructBase* Clone() const override { return new FZeroSlideState(*this); }
    
    virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
    {
       Super::NetSerialize(Ar, Map, bOutSuccess);
       Ar << SlideDirection;
       Ar << TimeSpentSliding;
       bOutSuccess = true;
       return true;
    }

    virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
    {
       const FZeroSlideState* AuthState = static_cast<const FZeroSlideState*>(&AuthorityState);
       return !SlideDirection.Equals(AuthState->SlideDirection, 0.01f) || 
             !FMath::IsNearlyEqual(TimeSpentSliding, AuthState->TimeSpentSliding, 0.05f);
    }

    virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
    {
       const FZeroSlideState* FromState = static_cast<const FZeroSlideState*>(&From);
       const FZeroSlideState* ToState = static_cast<const FZeroSlideState*>(&To);
       SlideDirection = FMath::Lerp(FromState->SlideDirection, ToState->SlideDirection, Pct);
       TimeSpentSliding = FMath::Lerp(FromState->TimeSpentSliding, ToState->TimeSpentSliding, Pct);
    }
};
USTRUCT(BlueprintType)
struct FZeroDashState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Zero|Inputs")
	FVector MoveInput = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite, Category = "Zero|State")
	bool bIsCurrentlyDashing = false;

	UPROPERTY(BlueprintReadWrite, Category = "Zero|State")
	float DashTimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Zero|State")
	FVector ReplicatedDashDirection = FVector::ZeroVector;
	
	// The game timestamp (in seconds) when the player can next dash
	UPROPERTY(BlueprintReadWrite, Category = "Zero|State")
	float NextDashAvailableTime = 0.0f;

	virtual UScriptStruct* GetScriptStruct() const override { return FZeroDashState::StaticStruct(); }
	virtual FMoverDataStructBase* Clone() const override { return new FZeroDashState(*this); }
    
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Super::NetSerialize(Ar, Map, bOutSuccess);
		
		Ar << MoveInput;
		Ar << NextDashAvailableTime;
		bOutSuccess = true;
		return true;
	}

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FZeroDashState* AuthState = static_cast<const FZeroDashState*>(&AuthorityState);
		// Reconcile if timestamps mismatch significantly
		return FMath::Abs(NextDashAvailableTime - AuthState->NextDashAvailableTime) > 0.01f;
	}
};
UCLASS(BlueprintType)
class ZEROLOCK_API UZeroMovementSettings : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
    float SlideMinSpeed = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
    float SlideInitialSpeedBoost = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
    float SlideFriction = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
    float MaxSlideDuration = 1.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
    float SlideGravityForce = 10.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Jump")
    int32 MaxAirJumps = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Jump")
    float AirJumpForce = 600.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air Jump")
    float AirJumpLateralBoost = 300.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Jump")
    float WallJumpOffForce = 500.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Jump")
    float WallJumpVerticalForce = 700.0f; 
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	float DashSpeed =5000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	float DashCooldown =0.1;
};