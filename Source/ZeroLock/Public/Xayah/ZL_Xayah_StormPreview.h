// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Xayah_StormPreview.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * Featherstorm's aim marker: a flat circle on the crosshair point, as wide as the cone is there. Local only (never
 * replicated), spawned on the casting client with Instigator = Xayah.
 */
UCLASS()
class ZEROLOCK_API AZL_Xayah_StormPreview : public AActor
{
	GENERATED_BODY()

public:
	AZL_Xayah_StormPreview();

	virtual void Tick(float DeltaSeconds) override;

	void InitPreview(float InConeHalfAngle, float InMaxRange);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<UStaticMeshComponent> CircleMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	FLinearColor CircleColor = FLinearColor(0.9f, 0.1f, 0.35f);

private:
	float ConeHalfAngle = 15.0f;
	float MaxRange = 2000.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CircleMaterial;
};
