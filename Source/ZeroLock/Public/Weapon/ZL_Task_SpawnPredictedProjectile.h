// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ZL_Task_SpawnPredictedProjectile.generated.h"

class AProjectile;
class AZero_BasePlayerController;
 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpawnPredictedProjectileDelegate, AProjectile*, SpawnedProjectile);
 
/**
 * Cached spawn information for predicted projectiles that are spawned after a delay. Projectile spawning is delayed
 * when the client's ping is higher than the maximum forward prediction ping to prevent projectiles from being
 * forward-predicted too far.
 */
USTRUCT()
struct FDelayedProjectileInfo
{
	GENERATED_BODY()
 
	UPROPERTY()
	TSubclassOf<AProjectile> ProjectileClass;
 
	UPROPERTY()
	FVector SpawnLocation;
 
	UPROPERTY()
	FRotator SpawnRotation;
 
	UPROPERTY()
	TWeakObjectPtr<AZero_BasePlayerController> ZeroPC;
 
	UPROPERTY()
	uint32 ProjectileId;
 
	FDelayedProjectileInfo() :
		ProjectileClass(nullptr),
		SpawnLocation(ForceInit),
		SpawnRotation(ForceInit),
		ZeroPC(nullptr),
		ProjectileId(0)
	{}
};
 
 
 
/**
 * Target data for spawning projectiles. Used to send spawn information from the client to the server.
 */
USTRUCT()
struct FGameplayAbilityTargetData_ProjectileSpawnInfo : public FGameplayAbilityTargetData
{
	GENERATED_BODY()
 
	/** Location to spawn projectile at. */
	UPROPERTY()
	FVector SpawnLocation;
 
	/** Rotation with which to spawn projectile. */
	UPROPERTY()
	FRotator SpawnRotation;
 
	/** The projectile's ID. Used to link fake and authoritative projectiles. */
	UPROPERTY()
	uint32 ProjectileId;
	
	FGameplayAbilityTargetData_ProjectileSpawnInfo() :
		SpawnLocation(ForceInit),
		SpawnRotation(ForceInit),
		ProjectileId(0)
	{}
 
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayAbilityTargetData_ProjectileSpawnInfo::StaticStruct();
	}
 
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FGameplayAbilityTargetData_ProjectileSpawnInfo: (%i)"), ProjectileId);
	}
 
	static FGameplayAbilityTargetDataHandle MakeProjectileSpawnInfoTargetData(const FVector& SpawnLocation, const FRotator& SpawnRotation, const uint32 ProjectileId)
	{
		FGameplayAbilityTargetData_ProjectileSpawnInfo* TargetData = new FGameplayAbilityTargetData_ProjectileSpawnInfo();
		TargetData->SpawnLocation = SpawnLocation;
		TargetData->SpawnRotation = SpawnRotation;
		TargetData->ProjectileId = ProjectileId;
		FGameplayAbilityTargetDataHandle Handle;
		Handle.Data.Add(TSharedPtr<FGameplayAbilityTargetData_ProjectileSpawnInfo>(TargetData));
		return Handle;
	}
 
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << SpawnLocation;
		Ar << SpawnRotation;
		Ar << ProjectileId;
 
		bOutSuccess = true;
		return true;
	}
};
 
template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_ProjectileSpawnInfo> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_ProjectileSpawnInfo>
{
	enum
	{
		WithNetSerializer = true
	};
};
 
 
 
/**
 * predictively spawns an actor of type AProjectile. Should only be called in Local Predicted abilities.
 *
 * Projectile actors perform their own predictive reconciliation once spawned. This task links them together, and also
 * performs additional reconciliation by listening for ability system prediction rejection (e.g. destroying the
 * predicted actor if the ability activation was rejected on the server).
 *
 * Predicted projectiles should ideally never be spawned outside of this task. This task ensures proper initialization
 * of and linkage between the client's and the server's projectiles.
 *
 * Note that this task will always use the parameters given by the local client. If you, for example, use the player's
 * current aim location and rotation to spawn the projectile, this task will use the client's version of those
 * variables, which is usually the desired behavior.
 *
 * @see AProjectile
 */
UCLASS()
class ZEROLOCK_API UZL_Task_SpawnPredictedProjectile : public UAbilityTask
{
	GENERATED_BODY()
 
	/**
	 * Called locally when the projectile is spawned.
	 *
	 * On clients, this will return the fake projectile actor. On the server, this will return the authoritative
	 * projectile actor.
	 *
	 * Note that this delegate may have a volatile delay at higher latencies (when projectiles are delayed), so this
	 * should not be used for gameplay logic. It is intended primarily for cosmetic effects and validation.
	 */
	UPROPERTY(BlueprintAssignable)
	FSpawnPredictedProjectileDelegate Success;
 
	/** Called on the client and server if either failed to spawn their projectile actor, usually because the ability's
	 * prediction key was rejected. The ability should likely be cancelled (on both machines) at this point. */
	UPROPERTY(BlueprintAssignable)
	FSpawnPredictedProjectileDelegate FailedToSpawn;
 
	/**
	 * Spawn a predicted projectile actor. Should only be used in Local Predicted abilities. To spawn non-predicted
	 * projectiles, use a normal "Spawn Actor" task.
	 *
	 * Note that this task will always use the parameters given by the local client. For example, if you use the
	 * player's current aim location and rotation to spawn the projectile, this task will use the client's version of
	 * those variables.
	 */
	UFUNCTION(BlueprintCallable, Meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "True"), Category="Ability|Tasks")
	static UZL_Task_SpawnPredictedProjectile* SpawnPredictedProjectile(UGameplayAbility* OwningAbility, TSubclassOf<AProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation);
 
	/** Spawn a fake projectile on the client and an authoritative projectile on the server. The server's projectile
	 * will be forward predicted to sync with the client's, the projectiles will be linked together, and reconciliation
	 * will be performed by the projectiles themselves. */
	virtual void Activate() override;
 
 
 
	// Internals.
 
protected:
 
	/** Handle for spawning a fake projectile after a delay, when the client's ping is higher than the
	 * forward-prediction limit. */
	FTimerHandle SpawnDelayedFakeProjHandle;
 
	/** Cached spawn info for spawning a fake projectile after a delay. */
	UPROPERTY()
	FDelayedProjectileInfo DelayedProjectileInfo;
 
	/** Spawns a fake projectile using the DelayedProjectileInfo. */
	void SpawnDelayedFakeProjectile();
 
	/** Replicates the client's spawn data to the server, so the server can spawn the authoritative projectile. */
	void SendSpawnDataToServer(const FVector& InLocation, const FRotator& InRotation, uint32 InProjectileId);
 
	/** Sends the task cancellation to the server if the client failed. */
	void CancelServerSpawn();
 
	/** Spawns the authoritative projectile on the server when the spawn data is received from the client. */
	void OnSpawnDataReplicated(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag Activation);
 
	/** Cancels this task on the server if the client failed to spawn their version of the projectile. */
	void OnSpawnDataCancelled();
 
	/** Destroys the client's fake projectile if this task is rejected (e.g. the server rejects the ability
	 * activation). */
	UFUNCTION()
	void OnTaskRejected();
 
	/** The fake projectile spawned by this task on the client. Used to destroy the spawned projectile if this task is
	 * rejected. */
	TWeakObjectPtr<AProjectile> SpawnedFakeProj;
 
	/** Helper to make spawn parameters for a projectile. */
	FActorSpawnParameters GenerateSpawnParams() const;
 
	/** Helper to make spawn parameters for a fake projectile. */
	FActorSpawnParameters GenerateSpawnParamsForFake(const uint32 ProjectileId) const;
 
	/** Helper to make spawn parameters for an authoritative projectile. */
	FActorSpawnParameters GenerateSpawnParamsForAuth(const uint32 ProjectileId) const;
 
 
 
	// Parameters.
 
protected:
 
	/** The projectile class to spawn. */
	UPROPERTY()
	TSubclassOf<AProjectile> ProjectileClass;
	
	UPROPERTY()
	TSubclassOf<AProjectile> ServerProjectileClass;
 
	/** Location at which to spawn the projectile. Determined by client. */
	FVector SpawnLocation;
 
	/** Rotation with which to spawn the projectile. Determined by client. */
	FRotator SpawnRotation;
};