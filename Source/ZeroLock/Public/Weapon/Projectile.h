// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

#include "Weapon/ZeroAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Projectile.generated.h"

class AZero_BasePlayerController;
class UArrowComponent;
class UCapsuleComponent;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;
 
/**
*
*	============================== Projectile Prediction ==============================
*
*	Documentation is available here:
*		https://docs.google.com/document/d/1VBhB41mwQWksoPLgx-G8YSelnWQ7_FYu4-QGueqY2lY/edit?usp=sharing
*
*	A more detailed breakdown is available here:
*		https://sreitich.github.io/projectile-prediction-4/
*
*	@see UAbilityTask_SpawnPredictedProjectile
*	@see ACrashPlayerController
*	@see UGASDeveloperSettings
*
*/
 
 
 
/**
 * Replicated movement data for projectiles' root components. More efficient than FRepMovement.
 */
USTRUCT()
struct FRepProjectileMovement
{
	GENERATED_BODY()
 
	UPROPERTY()
	FVector_NetQuantize LinearVelocity;
 
	UPROPERTY()
	FVector_NetQuantize Location;
 
	UPROPERTY()
	FRotator Rotation;
 
	FRepProjectileMovement()
		: LinearVelocity(ForceInit)
		, Location(ForceInit)
		, Rotation(ForceInit)
	{}
 
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;
 
		bool bOutSuccessLocal = true;
 
		Location.NetSerialize(Ar, Map, bOutSuccessLocal);
		bOutSuccess &= bOutSuccessLocal;
		Rotation.SerializeCompressed(Ar);
		LinearVelocity.NetSerialize(Ar, Map, bOutSuccessLocal);
		bOutSuccess &= bOutSuccessLocal;
 
		return bOutSuccess;
	}
 
	bool operator==(const FRepProjectileMovement& Other) const
	{
		if (LinearVelocity != Other.LinearVelocity)
		{
			return false;
		}
 
		if (Location != Other.Location)
		{
			return false;
		}
 
		if (Rotation != Other.Rotation)
		{
			return false;
		}
 
		return true;
	}
 
	bool operator!=(const FRepProjectileMovement& Other) const
	{
		return !(*this == Other);
	}
};
 
 
 
/**
 * Information about how the authoritative projectile detonated, replicated to clients to use once they catch up to the
 * server.
 */
USTRUCT()
struct FDetonationInfo
{
	GENERATED_BODY()
 
	UPROPERTY()
	bool bDetonated;
 
	UPROPERTY()
	bool bHasDirectImpactTarget;
 
	UPROPERTY()
	TObjectPtr<AActor> OtherActor;
 
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> OtherComp;
 
	UPROPERTY()
	FVector HitLocation;
 
	UPROPERTY()
	FVector HitNormal;
 
	FDetonationInfo()
		: bDetonated(false)
		, bHasDirectImpactTarget(false)
		, OtherActor(nullptr)
		, OtherComp(nullptr)
		, HitLocation(ForceInit)
		, HitNormal(ForceInit)
	{}
 
	FDetonationInfo(
		bool bInDetonated,
		bool bInHasDirectImpactTarget,
		AActor* InOtherActor,
		UPrimitiveComponent* InOtherComp,
		const FVector& InHitLocation,
		const FVector& InHitNormal)
		: bDetonated(bInDetonated)
		, bHasDirectImpactTarget(bInHasDirectImpactTarget)
		, OtherActor(InOtherActor)
		, OtherComp(InOtherComp)
		, HitLocation(InHitLocation)
		, HitNormal(InHitNormal)
	{}
 
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;
 
		bool bOutSuccessLocal = true;
 
		Ar << bDetonated;
		Ar << bHasDirectImpactTarget;
		Ar << OtherActor;
		Ar << OtherComp;
		HitLocation.NetSerialize(Ar, Map, bOutSuccessLocal);
		bOutSuccess &= bOutSuccessLocal;
		HitNormal.NetSerialize(Ar, Map, bOutSuccessLocal);
		bOutSuccess &= bOutSuccessLocal;
 
		return bOutSuccess;
	}
};
 
template<>
struct TStructOpsTypeTraits<FDetonationInfo> : public TStructOpsTypeTraitsBase2<FDetonationInfo>
{
	enum
	{
		WithNetSerializer = true
	};
};
 
 
 
/**
 * How to determine the direction in which to apply effects from projectiles.
 */
UENUM()
enum class EEffectDirection : uint8
{
	// Apply effects in the direction that the projectile is facing.
	InProjectileDirection,
 
	// Apply effects in the direction that the projectile is moving. Will differ from InProjectileDirection if the
	// projectile is rotating or "Rotation Follows Velocity" is false.
	InVelocityDirection,
 
	// Apply effects in the direction from the projectile's position to the target.
	FromProjectilePosition,
};
 
 
 
/**
 * Base class for projectiles. This is primarily used for predicted projectiles, but non-predicted projectiles can also
 * inherit from this class to re-use its hit detection and activation logic.
 *
 * Predicted projectiles should ONLY be spawned on the local client and server with the "Spawn Predicted Projectile"
 * ability task in Local Predicted abilities. This task is responsible for properly spawning, initializing, linking, and
 * reconciling predicted projectiles.
 *
 * Non-predicted projectiles can be spawned manually (e.g. with a normal "Spawn Actor from Class" node), but this should
 * only be done on the server. The projectile will replicate to clients and behave as expected. Spawning a non-predicted
 * projectile directly on a client will lead to undesirable behavior and is not allowed. If you want to create a purely
 * cosmetic projectile, do not derive from this class.
 *
 * This actor's owner should be the owning actor of the ASC that spawned it. The instigator should be the ASC's avatar.
 * Projectiles not spawned by players or AI (e.g. environmental hazards) should be spawned without an instigator or
 * owner.
 */
UCLASS(Abstract, Config = Game, Meta = (ChildCanTick, ToolTip = "Base class for projectiles."))
class ZEROLOCK_API AProjectile : public AActor
{
	GENERATED_BODY()
 
	// Initialization.
 
public:
 
	/** Default constructor. */
	AProjectile(const FObjectInitializer& ObjectInitializer);
 
	/** Replicates this projectile's spawn location to simulated proxies so they can resimulate it. */
	virtual void OnConstruction(const FTransform& Transform) override;
 
	/** Initialize our ASC and player controller to replicate them to clients. */
	virtual void PreInitializeComponents() override;
 
	/** Tries to link the client's version of the authoritative projectile to its corresponding fake projectile. */
	virtual void BeginPlay() override;
 
	/** Tick client's version of the authoritative projectile forward to where it is on the server, to make up for
	 * replication time. */
	void CatchupTick(float CatchupTickDelta);
 
	/** Allows LifeSpan to be simulated on the client-side. Default implementation only tracks LifeSpan on the server,
	 * but this makes simulating projectile effects easier on clients. */
	virtual void SetLifeSpan(float InLifespan) override;
 
	/** Forcefully detonates the projectile locally when it detonates on the server, if it hasn't already. */
	virtual void TornOff() override;
 
	/** If this projectile hasn't detonated when its lifespan end, forcefully detonate it if bDetonateOnLifeSpanEnd is
	 * enabled. */
	virtual void LifeSpanExpired() override;
 
	/**
	 * Disables this projectile (collision, movement, ambient effects, etc.) while it's pending destruction.
	 *
	 * For replication purposes, there's a delay between the projectile's impact/detonation and its destruction. This
	 * function visually destroys the projectile before the actor is actually killed.
	 */
	void ShutDown();
 
	/** Blueprint hook for ShutDown. */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "ShutDown", Meta = (ToolTip = "Disables this projectile (FX, collision, physics, etc.) while its pending destruction. Should be used instead of EndPlay (since there's a delay before projectiles are actually destroyed, for replication purposes)."))
	void K2_ShutDown();
 
	/** Destroys the fake projectile whenever the authoritative projectile is destroyed. */
	virtual void Destroyed() override;
 
	/** Purge projectiles on level reload. */
	virtual void Reset() override { Destroy(); }
 
protected:
 
	/** True once fully spawned, to avoid destroying replicated projectiles before they've had a chance to link to the
	 * fake projectile (which prevents attempts to reconcile missed predictions). */
	UPROPERTY()
	bool bHasSpawnedFully;
 
	/** Delay the projectile tear-off to make sure it had time to replicate its initial state. */
	FTimerHandle TearOffTimer;
 
private:
 
	/** Safely disables and hides this projectile without destroying it. */
	void DisableAndHide();
 
 
 
	// Prediction.
 
public:
 
	/** Initialize this projectile as the fake projectile. */
	void InitFakeProjectile(AZero_BasePlayerController* OwningPlayer, uint32 InProjectileId);
 
	/** Initialize this projectile's ID. */
	FORCEINLINE void InitProjectileId(uint32 InProjectileId) { ProjectileId = InProjectileId; }
 
	/** Whether this projectile is a fake client-side projectile. Set before BeginPlay. */
	bool bIsFakeProjectile;
protected:
 
	
 
	/** This projectile's ID. Used to link fake and authoritative projectiles. Only valid on the owning client (i.e. the
	 * client with the fake projectile). NULL_PROJECTILE_ID on other machines. Set before BeginPlay. */
	// TODO: Switch to TOptional once Unreal supports replication for it.
	UPROPERTY(Replicated)
	uint32 ProjectileId;
 
	/** The fake projectile that is representing this actor on the owning client, if this actor is the authoritative
	 * server-side projectile. */
	UPROPERTY()
	TObjectPtr<AProjectile> LinkedFakeProjectile;
 
	/** The authoritative server-side projectile that is representing this actor on the server and remote clients, if
	 * this actor is the fake client-side projectile. */
	UPROPERTY()
	TObjectPtr<AProjectile> LinkedAuthProjectile;
 
	/** If true, the fake projectile will lerp towards the position of the real projectile over time until they're
	 * synchronized. Only recommended for slow-moving projectiles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bCorrectFakeProjectilePositionOverTime;
 
	/** Link this authoritative projectile with its corresponding fake projectile. */
	void LinkFakeProjectile(AProjectile* InFakeProjectile);
 
	/** When called on an authoritative projectile, switches to using that authoritative projectile and destroys the
	 * fake projectile. Useful for missed predictions, when a fake projectile mistakenly detonates, and we want to
	 * reconcile by switching to using the real one. */
	void SwitchToRealProjectile();
 
	/** Lerps the fake projectile towards the real one each tick if bCorrectFakeProjectilePositionOverTime is true. */
	virtual void Tick(float DeltaSeconds) override;
 
	/** Called each tick to lerp the fake projectile towards the authoritative one. */
	void CorrectionLerpTick(float DeltaTime);
 
private:
 
	/** The initial distance from the fake projectile to its corresponding authoritative projectile. Used to correct the
	 * fake projectile's position over time. */
	float InitialProjectileError;
 
	/** After the fake projectile detonates, if the real projectile doesn't detonate in time, we switch to it and
	 * consider the fake projectile's detonation a missed prediction. */
	FTimerHandle SwitchToAuthTimer;
 
 
 
	// Collision.
 
protected:
 
	/** Root collision component. This determines collision with the environment and the projectile's movement. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComp;
 
	/**
	 * Overlap component used to detect hits against targets (usually pawns). This is separate from CollisionComp
	 * because we usually want our hitbox to be bigger than the projectile's collider. E.g. a spear projectile's
	 * collision component should be the size of its tip so it can pass through small gaps, but its hitbox should much
	 * more generous.
	 *
	 * We use a capsule component instead of a sphere component so we can have a little more flexibility with our hitbox
	 * shapes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UCapsuleComponent> HitboxComp;
 
 
 
	// Movement.
 
public:
 
	/** Projectile movement component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
 
	/** Arrow to help indicate projectile direction in editor. */
	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComp;
 
	/** If true, this projectile will only be allowed to bounce a certain number of times. If it impacts a surface AFTER
	 * bouncing the maximum number of times, it will stop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ProjectileBounces", Meta = (EditCondition = "ProjectileMovement->bShouldBounce"))
	bool bLimitBounces;
 
	/** The maximum number of times this projectile can bounce, if LimitBounces is enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ProjectileBounces", Meta = (EditCondition = "bLimitBounces"))
	int32 MaximumBounces;
 
	/**
	 * If enabled, this projectile's velocity will be updated to face its owning player's camera vector on its first
	 * bounce.
	 *
	 * This is useful for projectiles that are thrown at an angle (e.g. from a character's hand to where the player is
	 * aiming), so they don't continue traveling at that angle, past the player's camera vector, after reaching their
	 * desired target location and bouncing. Instead, the projectile will travel to the player's desired location and
	 * bounce straight forward along the player's aim vector.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ProjectileBounces", Meta = (EditCondition = "ProjectileMovement->bShouldBounce"))
	bool bStraightenProjectileOnBounce;
 
	/** Whether this projectile has detonated or not. Prevents multiple applications of effects and helps detect missed
	 * predictions. */
	bool bDetonated;
 
	/** Re-entrance guard for OnHitboxOverlapBegin. */
	bool bInOverlap;
 
	/** Called when this projectile bounces off a surface. Triggers the Bounce FX. */
	UFUNCTION()
	virtual void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);
 
	/** Called when this projectile's hitbox component overlaps an actor. Triggers detonation if the overlapping actor
	 * is a valid target. */
	UFUNCTION()
	virtual void OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
 
	/** Called when this projectile stops moving (usually because the collision component hit a blocking surface).
	 * Triggers detonation. */
	UFUNCTION()
	virtual void OnStop(const FHitResult& Hit);
	
	/** Disable killbox destruction for projectiles. All projectiles have a finite lifespan, so they never run the risk
	 * of lingering forever. We don't want the world destroying a projectile without us wanting it gone. */
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
 
 
 
	// Movement replication.
 
public:
 
	/** Gather replicated movement if bReplicateProjectileMovement is set. */
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
 
	/** Fills ReplicatedProjectileMovement. */
	virtual void GatherCurrentMovement() override;
 
	/** Apply replicated velocity to this projectile's movement component. */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
 
	/** Use this projectile's root component's velocity as the actor velocity. */
	virtual FVector GetVelocity() const override;
 
	/** Remove the IsReplicatingMovement() condition. We use bReplicateProjectileMovement instead. */
	virtual void OnRep_ReplicatedMovement() override;
 
	/** Remove the IsReplicatingMovement() condition from AttachmentReplication. */
	virtual void GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const override;
 
protected:
 
	/** Enables movement replication. Use this instead of bReplicateMovement. This should usually not be enabled by
	 * default, because we want projectiles simulate locally. Its primary use is replicating the final position of the
	 * authoritative projectile to clients, to ensure they detonate in the correct location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replication")
	bool bReplicateProjectileMovement;
 
	/** Used to replicate projectile movement. */
	UPROPERTY(ReplicatedUsing = OnRep_RepProjectileMovement)
	FRepProjectileMovement ReplicatedProjectileMovement;
 
	/** Applies replicated projectile movement on clients. */
	UFUNCTION()
	void OnRep_RepProjectileMovement();
 
	/** Timer to defer this projectile's detonation on non-owning simulated proxies. We wait until the projectile has
	 * finished resimulating to trigger its detonation. */
	FTimerHandle FinishedResimulationTimer;
 
	/** Whether this projectile, if a non-owning simulated proxy, has finished locally resimulating the real
	 * projectile, and can safely be detonated (without the risk of detonating when the real projectile didn't). */
	bool bFinishedResim;
 
 
 
	// Gameplay effects.
 
protected:
 
	/** Whether to filter hit actors with Filter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bUseFilter;
 

 
	/** The gameplay effect to apply when this projectile hits a direct target (as opposed to an AOE target). Put a cue
	 * trigger in this effect if you want impact FX. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<UGameplayEffect> ImpactGameplayEffect;
 
	/** Which direction to apply the impact gameplay effect in. Used for gameplay effects (e.g. direction of knockback),
	 * not VFX. (Area effects are always applied with FromProjectilePosition.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", Meta = (EditCondition = "ImpactGameplayEffect != nullptr"))
	EEffectDirection ImpactEffectDirection;
 
	/** Called when this projectile stops because it hit a valid target, hit a blocking surface, or stopped moving (e.g.
	 * ran out of bounces). Applies ImpactGameplayEffect to the hit actor (if bHasDirectImpactTarget is true), triggers
	 * VFX and optional AOE effects, and shuts down this projectile. This is the final projectile event replicated to
	 * clients, and is synced across all machines. */
	UFUNCTION()
	void Detonate(bool bHasDirectImpactTarget, AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);
 
	/**
	 * Blueprint hook for post-detonation effects (e.g. impaling a spear projectile into the hit target).
	 *
	 * @param bHasDirectImpactTarget		Whether this projectile detonated against a valid target using its hitbox
	 *										component, as opposed to hitting the environment.
	 * @param HitActor						The actor that the projectile detonated against. If HasDirectImpactTarget
	 *										and UseFilter are both true, then this actor is guaranteed to have an ASC.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta = (ToolTip = "Called when the projectile detonates. \n\nThis is only called when this projectile's FX are triggered, so it will always fire exactly once per projectile on each client, and can be safely used to execute additional FX or gameplay. If \"Predict FX\" is enabled, this will be fired predictively, and may be fired twice due to missed predictions (the second time being on the authoritative projectile)."))
	void OnDetonate(bool bHasDirectImpactTarget, AActor* HitActor, UPrimitiveComponent* HitComp, FVector HitLocation, FVector HitNormal);
 
	/** If this is the owning client's version of the authoritative projectile, whether it should trigger FX (i.e.
	 * visuals, audio, etc.). This function is primarily checking for missed predictions. */
	bool ShouldAuthProjDetonateOnOwner(bool bLog = false) const;
 
	/**
	 * Constructs an effect spec to apply a gameplay effect to a hit target. Override to use a dynamic GE class or to
	 * set a set-by-caller magnitude.
	 *
	 * Default implementation passes the "Impact" GE or "Area of Effect" GE (depending on "Direct Impact") into "Make
	 * Outgoing Spec With Hit Result," using this actor as the source object. Both GE properties must be set for any
	 * effects to be applied.
	 *
	 * Note that this projectile's instigating ASC can be retrieved through its owner. The target's ASC can be used if
	 * this projectile was spawned by an actor without one.
	 */
	UFUNCTION(BlueprintNativeEvent)
	FGameplayEffectSpecHandle MakeEffectSpec(bool bDirectImpact, const AActor* Target, const FHitResult& Hit) const;
 
// Area of effect.
protected:
 
	/** This projectile's area-of-effect radius. If greater than 0, the AOE Gameplay Effect will be applied to all
	 * actors within this radius on detonation. Set to 0 to disable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area of Effect", DisplayName = "Area of Effect Radius", Meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AreaRadius;
 
	/** The relative offset (from the center of the collision component) where the center of the AOE radius will be. Z
	 * offset is applied in world space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area of Effect", DisplayName = "Area of Effect Offset", Meta = (EditCondition = "AreaRadius > 0.0"))
	FVector AreaOffset;
 
	/** GE to apply to actors within AoE Radius. To apply attenuation, implement the EffectSourceInterface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area of Effect", DisplayName = "Area of Effect Gameplay Effect", Meta = (EditCondition = "AreaRadius > 0.0"))
	TSubclassOf<UGameplayEffect> AreaGameplayEffect;
 
	/** If true, this projectile will NOT apply the AOE gameplay effect to targets it hits directly. Useful if you want
	 * to apply different effects to targets this projectile hits directly, and targets within its explosion radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area of Effect", DisplayName = "Skip AOE Effect for Impact Target?", Meta = (EditCondition = "AreaRadius > 0.0"))
	bool bSkipAreaEffectForImpactTarget;
 
	/** Helper to calculate the origin of this projectile's AOE radius, if it has one. */
	FORCEINLINE FVector GetAreaOfEffectOrigin() const;
 
private:
 
	/** Helper for applying a gameplay effect (ImpactGameplayEffect or AreaGameplayEffect) to a target actor. */
	void ApplyEffectToTarget(const bool bDirectImpact, const AActor* Target, const FHitResult& Hit) const;
 
 
 
	// Lifetime.
 
protected:
 
	/**
	 * How long to keep the projectile alive after it detonates (e.g. to allow it to play a "landed" animation or to
	 * stick in a wall). Note that this will not extend the lifespan of projectiles whose lifespan expires without
	 * detonating.
	 *
	 * This is clamped internally to make sure the projectile has enough time to finish replicating before dying. If
	 * this is set to a value lower than the minimum life span required by this projectile, the projectile will instead
	 * be hidden after this timer expires, until it can be safely destroyed.
	 *
	 * NOTE: Using this with bDetonateOnLifeSpanEnd enabled has not been fully tested.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", Meta = (Units = "Seconds"))
	float LifeSpanAfterDetonation;
 
	/** If true, when this projectile's lifespan ends, it will detonate (without a direct impact target) if it hasn't
	 * done so already. Useful for projectiles with a "fuse." */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bDetonateOnLifeSpanEnd;
 
 
 
	// FX.
 
protected:
 
	/** Whether this projectile's impact and detonation FX should be predicted. We generally don't want to predict
	 * big or slow projectiles with large AOE effects (like a rocket's explosion VFX), but we can predict the impact of
	 * smaller, fast projectiles that don't have AOE effects (like a shuriken's missed impact VFX). */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	bool bPredictFX;
 
	/** Spawned when this projectile detonates without a target (i.e. when it hits the environment). To spawn FX when
	 * this projectile detonates WITH a target, put a cue in the Impact Gameplay Effect. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	FProjectileFX MissedImpactFX;
 
	/** Spawned when this projectile detonates (regardless of whether it hit a surface or a target). */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	FProjectileFX DetonationFX;
 
	/** Spawned when this projectile bounces. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	FProjectileFX BounceFX;
 
	/** Minimum velocity at which this projectile will trigger FX when bouncing. Note that this is relative to the
	 * normal of the bounce, to prevent sliding. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
	float BounceVelocityFXThreshold;
 
	/** Timer to visually shut down this projectile (hide its meshes, kill its VFX, etc.) before destroying it.
	 * Projectiles' destruction is delayed to ensure they have time to replicate, but they should usually be visually
	 * destroyed before then. */
	FTimerHandle ShutDownTimer;
 
	/** Whether this projectile was used for FX. If a projectile isn't used for FX, it will always be hidden (or
	 * destroyed, if safe) when shut down. Otherwise, it will be kept visible according to LifeSpanAfterDetonation. */
	bool bTriggeredFX;
 
 
 
	// Internals.
 
protected:
 
	/** The owning player's controller, cached in case our instigator dies during the projectile's lifetime. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Projectile")
	TWeakObjectPtr<AZero_BasePlayerController> ZeroPlayerController;
 
	/** The transform with which this projectile was spawned. Used to resimulate the projectile on non-owning simulated
	 * proxies, to prevent projectiles from appearing far ahead of where they were spawned. */
	UPROPERTY(Replicated)
	FTransform SpawnTransform;
 
	/** The control rotation of this projectile's owner (i.e. the owning player's camera vector) when it was spawned.
	 * Will be the same as this projectile's spawn rotation if it was spawned at the player's camera. Used to
	 * "straighten out" bouncing projectiles' movement after the first bounce, if desired.
	 * @see bStraightenProjectileOnBounce */
	UPROPERTY(Replicated)
	FRotator InitialOwnerAimRotation;
 
	/** Whether this actor has hit its first bounce yet. */
	bool bHasBounced;
 
	/** How many times this projectile has bounced so far. */
	int32 BounceCount;
 
	/** Information about this projectile's detonation, replicated to non-owning simulated proxies so they can properly
	 * detonate once they finish resimulating (since their resimulation will likely end at a slightly different time
	 * and location than the authoritative projectile). */
	UPROPERTY(ReplicatedUsing = OnRep_DetonationInfo)
	FDetonationInfo DetonationInfo;
 
	/** 
	 * Triggers detonation on non-owning simulated proxies. Since simulated proxies rewind and resimulate, they're
	 * always behind the authoritative projectile, which makes their local hit detection unreliable. Instead of letting
	 * them trigger their detonation locally, we wait for the server to tell us that the authoritative projectile
	 * detonated.
	 *
	 * If detonation info is sent by the server before the simulated proxy has finished spawning, we start our
	 * resimulation early (instead of waiting for BeginPlay) and adjust our resimulation speed to ensure the projectile
	 * is always at least briefly visible. Otherwise, we extrapolate how much time is left in our resimulation
	 * (depending on our distance from the detonation location) and set a timer to detonate when we finish.
	 */
	UFUNCTION()
	void OnRep_DetonationInfo();
 
	/** Calls Detonate using the current values of DetonationInfo at parameters. */
	void DetonateWithDetonationInfo();
 
	/** Whether this projectile is on the server. */
	FORCEINLINE bool IsServerProjectile() const;
 
private:
 
	/** How long to delay tearing off this projectile on the server to ensure the initial replication is sent first. */
	UPROPERTY(Config)
	float TearOffDelay;
 
	/** The minimum amount of time that this projectile needs to be visible. If this projectile detonates on spawn, it
	 * will be rewound and resimulated at a low speed to ensure it's at least briefly visible to players. */
	UPROPERTY(Config)
	float MinLifetime;
	

};
