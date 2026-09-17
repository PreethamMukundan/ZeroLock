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
    	Map->SerializeObject(Ar, AActor::StaticClass(), (UObject*&)DynamicTargetActor);
    
    	

       bOutSuccess = true;
       return true;
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