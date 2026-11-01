// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Solomon_BishopSquare.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class AZeroLockCharacter;

/**
 * One of the bishop's diagonal squares (AZL_Solomon_Bishop spawns them after a move). Replicated, no lifespan: it stays
 * until the bishop moves again or goes away, or until an enemy of its instigator steps into it (server): that enemy
 * takes SquareDamage spirit damage and the square is gone. Allies and Solomon walk over it.
 */
UCLASS()
class ZEROLOCK_API AZL_Solomon_BishopSquare : public AActor
{
	GENERATED_BODY()

public:
	AZL_Solomon_BishopSquare();

	/** Server, before FinishSpawning. Size = edge length (cm), Height = how high above the floor it catches heroes. */
	void InitSquare(float InSize, float InHeight, float InDamage);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Square")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Placeholder look: flat engine Cube tinted SquareColor. */
	UPROPERTY(VisibleAnywhere, Category = "Square")
	TObjectPtr<UStaticMeshComponent> SquareMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Square")
	FLinearColor SquareColor = FLinearColor(0.15f, 0.35f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Square")
	float SquareThickness = 3.0f;

	/** Gap between neighbouring squares' visuals (cm), so they read as separate tiles. */
	UPROPERTY(EditDefaultsOnly, Category = "Square")
	float VisualInset = 10.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Size)
	float SquareSize = 245.0f;

	float SquareHeight = 280.0f;
	float SquareDamage = 25.0f;
	bool bTriggered = false;

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Size();
	void ApplySize();

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryTrigger(AZeroLockCharacter* Villan);
};
