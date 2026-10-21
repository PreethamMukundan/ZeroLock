// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameFramework/Actor.h"
#include "ZL_Doorman_Hotel.generated.h"

class AZeroLockCharacter;
class AZL_Doorman_HotelPlaceholder;
class UBaseCharAbilitySystemComponent;
class UGameplayEffect;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

/** Why a hotel stay ended. */
UENUM(BlueprintType)
enum class EZL_HotelCheckout : uint8
{
	OnTime		UMETA(ToolTip = "Reached the exit elevator in time"),
	Late		UMETA(ToolTip = "Time limit ran out: LateCheckoutDamage, then sent back"),
	Died		UMETA(ToolTip = "Died during the stay: sent back before the death/respawn logic"),
	Cancelled	UMETA(ToolTip = "Guest or hotel went away (hero swap, disconnect, end of play)")
};

/** What a stay costs and how long it lasts (filled in by UZL_Doorman_HotelGuest). */
USTRUCT()
struct FZL_HotelStayParams
{
	GENERATED_BODY()

	float TimeLimit = 8.0f;
	float EntryDamage = 100.0f;
	float LateCheckoutDamage = 150.0f;
	int32 Level = 1;
	TSubclassOf<UGameplayEffect> GuestEffect;
	TSubclassOf<AZL_Doorman_HotelPlaceholder> PlaceholderClass;
};

/** Server-only state of one guest. */
USTRUCT()
struct FZL_HotelStay
{
	GENERATED_BODY()

	TWeakObjectPtr<AZeroLockCharacter> Guest;
	TWeakObjectPtr<AZeroLockCharacter> Doorman;
	TWeakObjectPtr<UBaseCharAbilitySystemComponent> DoormanASC;
	TWeakObjectPtr<AZL_Doorman_HotelPlaceholder> Placeholder;
	FVector ReturnLocation = FVector::ZeroVector;
	FRotator ReturnControlRotation = FRotator::ZeroRotator;
	int32 Lane = INDEX_NONE;
	double Deadline = 0.0;
	float LateCheckoutDamage = 0.0f;
	FActiveGameplayEffectHandle EffectHandle;
	FDelegateHandle PreDeathHandle;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FZL_OnHotelStayEnded, AZeroLockCharacter* /*Guest*/, EZL_HotelCheckout /*Reason*/);

/**
 * Doorman's Baroness Hotel. One per map: place it in the level, or the first Hotel Guest cast spawns one (GetOrSpawnHotel)
 * and it stays. Placeholder look: NumLanes enclosed corridors side by side (engine cubes, built in OnConstruction from the
 * size properties), one guest per lane. Each guest starts at the lane's start wall facing +X and must reach the exit
 * elevator at the far end before the time limit. Stays run on the server (Tick checks the guest's position against the
 * elevator box, since server overlaps for remote Mover pawns are unreliable). Put it far from the playable map.
 */
UCLASS(Blueprintable)
class ZEROLOCK_API AZL_Doorman_Hotel : public AActor
{
	GENERATED_BODY()

public:
	AZL_Doorman_Hotel();

	static constexpr int32 NumLanes = 4;

	/** Server: the hotel in this world, spawning HotelClass at SpawnLocation if there's none. */
	static AZL_Doorman_Hotel* GetOrSpawnHotel(UWorld* World, TSubclassOf<AZL_Doorman_Hotel> HotelClass, const FVector& SpawnLocation);

	/**
	 * Server: takes Guest's body to a free lane, leaves a placeholder, applies the guest effect, entry damage and the HUD
	 * timer. False if the hotel is full or Guest is already staying.
	 */
	bool CheckIn(AZeroLockCharacter* Doorman, AZeroLockCharacter* Guest, const FZL_HotelStayParams& Params);

	bool IsGuest(const AZeroLockCharacter* Guest) const;

	/** Server: broadcast after a guest is back in the map (or gone). */
	FZL_OnHotelStayEnded OnStayEnded;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Inside size of each corridor (cm). X = length, from the start wall to the end wall. */
	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float CorridorLength = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float CorridorWidth = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float CorridorHeight = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float WallThickness = 50.0f;

	/** Distance between lane centers along Y. */
	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float LaneSpacing = 1000.0f;

	/** Guests arrive this far from the start wall. */
	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float ArrivalOffset = 150.0f;

	/** Depth of the exit elevator at the end of each corridor; standing in it checks out. */
	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float ElevatorDepth = 250.0f;

	/** Extra margin around the elevator box for the server's position check. */
	UPROPERTY(EditAnywhere, Category = "Hotel|Layout")
	float ElevatorCheckTolerance = 30.0f;

	/** Meshes use this material's "Color" parameter (engine BasicShapeMaterial by default). */
	UPROPERTY(EditAnywhere, Category = "Hotel|Look")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, Category = "Hotel|Look")
	FLinearColor WallColor = FLinearColor(0.35f, 0.08f, 0.1f);

	UPROPERTY(EditAnywhere, Category = "Hotel|Look")
	FLinearColor FloorColor = FLinearColor(0.15f, 0.1f, 0.06f);

	UPROPERTY(EditAnywhere, Category = "Hotel|Look")
	FLinearColor ElevatorColor = FLinearColor(0.9f, 0.7f, 0.15f);

	UPROPERTY(EditAnywhere, Category = "Hotel|Look")
	float LightIntensity = 8000.0f;

	UPROPERTY(EditAnywhere, Category = "Hotel")
	bool bDrawDebug = false;

	UPROPERTY(VisibleAnywhere, Category = "Hotel")
	TArray<TObjectPtr<UStaticMeshComponent>> WallMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Hotel")
	TArray<TObjectPtr<UStaticMeshComponent>> FloorMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Hotel")
	TArray<TObjectPtr<UStaticMeshComponent>> ElevatorMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Hotel")
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	TArray<FZL_HotelStay> Stays;

	FVector GetLaneOrigin(int32 Lane) const;
	FVector GetArrivalLocation(int32 Lane, float GuestHalfHeight) const;
	FBox GetElevatorBox(int32 Lane) const;
	int32 FindFreeLane() const;

	void CheckOut(AZeroLockCharacter* Guest, EZL_HotelCheckout Reason);
	void HandleGuestPreDeath(AZeroLockCharacter* Guest);
	void ApplyColors();
};
