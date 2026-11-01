// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Solomon_QueenTile.generated.h"

class UStaticMeshComponent;

/**
 * Look of one of Develop Queen's line squares (AZL_Solomon_Queen deals the damage itself). Server-spawned, replicated,
 * lives TileLifetime. Placeholder flat engine Cube on the floor, SquareSize (RepNotify) minus VisualInset wide.
 */
UCLASS()
class ZEROLOCK_API AZL_Solomon_QueenTile : public AActor
{
	GENERATED_BODY()

public:
	AZL_Solomon_QueenTile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, before FinishSpawning. */
	void InitTile(float InSquareSize) { SquareSize = InSquareSize; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "QueenTile")
	TObjectPtr<UStaticMeshComponent> TileMesh;

	UPROPERTY(EditDefaultsOnly, Category = "QueenTile")
	FLinearColor TileColor = FLinearColor(0.85f, 0.1f, 0.6f);

	UPROPERTY(EditDefaultsOnly, Category = "QueenTile")
	float TileLifetime = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "QueenTile")
	float TileThickness = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "QueenTile")
	float VisualInset = 10.0f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_SquareSize)
	float SquareSize = 245.0f;

	UFUNCTION()
	void OnRep_SquareSize();
};
