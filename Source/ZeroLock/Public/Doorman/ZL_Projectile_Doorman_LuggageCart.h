// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Weapon/Projectile.h"
#include "ZL_Projectile_Doorman_LuggageCart.generated.h"

class AZeroLockCharacter;
class UGameplayEffect;

/**
 * Doorman's Luggage Cart: a predicted projectile (spawned with UZL_Task_SpawnPredictedProjectile) with its own ground
 * movement instead of ProjectileMovement. Every copy (fake, server, proxies) runs the same movement in Tick: drives
 * straight along its flat spawn heading, follows walkable ground and slopes (normal within MaxWalkableSlopeAngle of
 * up), falls with gravity when nothing is under it, steps up ledges up to MaxStepHeight. Anything else steeper is a
 * wall: the cart detonates there; on the server dragged enemies are stunned first. A linked Doorway door
 * (IZL_ProjectilePortal surface) sends the cart out of the other door instead. Server only: enemies the hitbox touches
 * take SpiritDamage once and are dragged (PullTo victim events) to DragPoint, stunned (CarryEffect) until released.
 * Detonates after MaxTravelDistance.
 */
UCLASS()
class ZEROLOCK_API AZL_Projectile_Doorman_LuggageCart : public AProjectile
{
	GENERATED_BODY()

public:
	AZL_Projectile_Doorman_LuggageCart(const FObjectInitializer& ObjectInitializer);

	/** Server: values from the owning ability. */
	void InitCart(float InSpiritDamage, TSubclassOf<UGameplayEffect> InStunEffect);

	virtual void Tick(float DeltaSeconds) override;

	float GetCartRadius() const { return CartRadius; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Visual only (no collision). Placeholder engine Cube sized to the cart; swap the mesh in the BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cart")
	TObjectPtr<UStaticMeshComponent> CartMesh;

	/** Where dragged enemies are held (their center). Move it in the BP; default in front of the cart. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cart|Drag")
	TObjectPtr<USceneComponent> DragPoint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Movement")
	float Speed = 1300.0f;

	/** The cart detonates after travelling this far (horizontal distance). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Movement")
	float MaxTravelDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Movement")
	float Gravity = 2500.0f;

	/** Surfaces at most this steep are floor/slope (the cart drives on them); steeper ones are walls. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Movement", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxWalkableSlopeAngle = 50.0f;

	/** Radius of the sphere swept against WorldStatic for movement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Movement")
	float CartRadius = 45.0f;

	/** How far below the cart ground is still followed (keeps it on downhill slopes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Movement")
	float GroundSnapDistance = 40.0f;

	/** Ledges (curbs, stair steps) up to this high are climbed instead of crashing. Keep below GroundSnapDistance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Movement")
	float MaxStepHeight = 35.0f;

	/** Radius of the hitbox that picks up enemies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Drag")
	float CaptureRadius = 130.0f;

	/** Seconds between PullTo updates for dragged enemies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Drag")
	float DragUpdateInterval = 0.1f;

	/** Fallbacks, overwritten by the owning ability (InitCart). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Damage")
	float SpiritDamage = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Damage")
	TSubclassOf<UGameplayEffect> StunEffect;

	/** Infinite effect granting ZeroLock.Stun while an enemy is carried (no moving, no abilities); removed on release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Drag")
	TSubclassOf<UGameplayEffect> CarryEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cart|Debug")
	bool bDrawDebug = false;

	FVector Heading = FVector::ForwardVector;
	float VerticalSpeed = 0.0f;
	float Travelled = 0.0f;

	struct FDraggedVillan
	{
		TWeakObjectPtr<AZeroLockCharacter> Villan;
		FActiveGameplayEffectHandle CarryHandle;
	};
	TArray<FDraggedVillan> Dragged;
	FTimerHandle DragTimer;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	bool IsAuthCart() const { return HasAuthority() && !bIsFakeProjectile; }
	bool IsWalkable(const FVector& Normal) const;
	bool SweepStatic(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	void MoveCart(float DeltaSeconds);
	bool TryStepUp(const FVector& From, const FVector& To, FVector& OutLocation) const;
	bool TryPortal(const FHitResult& Hit, const FVector& MoveDir);
	void GetDragHoldLocations(FVector& OutHold, FVector& OutHoldThroughDoor) const;
	void UpdateDrag();
	/** Server: remove the carry stun from everyone dragged and stop dragging. */
	void ReleaseDragged();
	void Crash(const FHitResult& WallHit);
	AZeroLockCharacter* GetHero() const;
};
