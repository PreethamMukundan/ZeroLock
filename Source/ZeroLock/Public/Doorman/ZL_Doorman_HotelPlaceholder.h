// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Doorman_HotelPlaceholder.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Marks where a hotel guest's body was taken from, until they check out (AZL_Doorman_Hotel spawns and destroys it).
 * Placeholder look: a capsule-sized cylinder with a door sign above it, both engine shapes tinted with Color.
 * No collision: it doesn't block, can't be shot.
 */
UCLASS(Blueprintable)
class ZEROLOCK_API AZL_Doorman_HotelPlaceholder : public AActor
{
	GENERATED_BODY()

public:
	AZL_Doorman_HotelPlaceholder();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> SignMesh;

	/** Body size; the actor's origin is the capsule center, like the guest's. */
	UPROPERTY(EditDefaultsOnly, Category = "Placeholder")
	float BodyRadius = 34.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Placeholder")
	float BodyHalfHeight = 88.0f;

	/** "Color" parameter of the meshes' material (engine BasicShapeMaterial by default). */
	UPROPERTY(EditDefaultsOnly, Category = "Placeholder")
	FLinearColor Color = FLinearColor(0.9f, 0.65f, 0.1f);

	UPROPERTY(EditDefaultsOnly, Category = "Placeholder")
	TObjectPtr<UMaterialInterface> Material;
};
