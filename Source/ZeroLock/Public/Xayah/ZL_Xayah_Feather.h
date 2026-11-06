// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Xayah_Feather.generated.h"

class AZeroLockCharacter;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UZL_Xayah_Bladecaller;

/**
 * A feather lying on the ground, left by Xayah's blades and empowered shots. Bladecaller calls it back to her.
 * Spawned by the server with Instigator = Xayah; replicated without movement: every machine flies it back on its own.
 */
UCLASS()
class ZEROLOCK_API AZL_Xayah_Feather : public AActor
{
	GENERATED_BODY()

public:
	AZL_Xayah_Feather();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: drops a feather on the floor under Location (at Location if there is no floor near). */
	static AZL_Xayah_Feather* Drop(TSubclassOf<AZL_Xayah_Feather> FeatherClass, AZeroLockCharacter* Hero, const FVector& Location);

	/** Server: sticks a feather into the surface that was hit, tip first (walls, floors and ceilings). */
	static AZL_Xayah_Feather* Stick(TSubclassOf<AZL_Xayah_Feather> FeatherClass, AZeroLockCharacter* Hero, const FHitResult& Surface);

	/** Her feathers on the ground, not the ones already flying back. */
	static TArray<AZL_Xayah_Feather*> GetGroundFeathers(const AActor* Hero);

	/** Server: flies back to Xayah. Enemies it passes are reported to Recall, once each. */
	void StartReturn(UZL_Xayah_Bladecaller* Recall);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feather")
	TObjectPtr<UStaticMeshComponent> FeatherMesh;

	/** Seconds on the ground before it fades. */
	UPROPERTY(EditDefaultsOnly, Category = "Feather")
	float GroundLifetime = 8.0f;

	/** Distance from the surface to its centre. */
	UPROPERTY(EditDefaultsOnly, Category = "Feather")
	float RestHeight = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Feather")
	float ReturnSpeed = 3500.0f;

	/** Width of the path that hits enemies on the way back. */
	UPROPERTY(EditDefaultsOnly, Category = "Feather")
	float ReturnHitRadius = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Feather")
	FLinearColor FeatherColor = FLinearColor(0.9f, 0.1f, 0.35f);

	UPROPERTY(ReplicatedUsing = OnRep_Returning)
	bool bReturning = false;

	UFUNCTION()
	void OnRep_Returning();

private:
	static AZL_Xayah_Feather* SpawnFeather(TSubclassOf<AZL_Xayah_Feather> FeatherClass, AZeroLockCharacter* Hero, const FVector& Location, const FRotator& Rotation);

	void HitEnemiesAlong(const FVector& Start, const FVector& End);

	TWeakObjectPtr<UZL_Xayah_Bladecaller> ReturnRecall;
	TSet<TWeakObjectPtr<AZeroLockCharacter>> HitVillans;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FeatherMaterial;
};
