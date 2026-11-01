// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZL_Solomon_MovePreview.generated.h"

class AZL_Solomon_ChessPiece;
class APlayerController;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Local-only marker for Move Piece (UZL_Solomon_MovePiece) while right mouse is held: every tick it traces the camera
 * crosshair against WorldStatic and shows the board square the selected piece would land on
 * (AZL_Solomon_ChessPiece::GetMoveTarget, the same rule the server applies): a flat tile the size of a square, ValidColor;
 * a circle of the move range around the piece (bDrawRange);
 * with no valid square it sits at the aim point in InvalidColor. Not replicated; spawned and destroyed by the ability on
 * the aiming player's machine.
 */
UCLASS()
class ZEROLOCK_API AZL_Solomon_MovePreview : public AActor
{
	GENERATED_BODY()

public:
	AZL_Solomon_MovePreview();

	void Init(AZL_Solomon_ChessPiece* InPiece, APlayerController* InPC, float InAimDistance);

	/** Crosshair point: camera trace against WorldStatic, or the end of the trace. */
	static FVector GetAimPoint(const APlayerController* PC, float AimDistance, const AActor* IgnoredActor);

	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Placeholder look: flat engine Cube sized to the piece's board square. */
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	FLinearColor ValidColor = FLinearColor(1.0f, 0.75f, 0.1f);

	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	FLinearColor InvalidColor = FLinearColor(0.9f, 0.1f, 0.1f);

	/** Also draw a line from the piece to the square. */
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	bool bDrawPath = true;

	/** Circle of the move range (MoveDistance) around the piece. Debug draw: development builds only. */
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	bool bDrawRange = true;

	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	FLinearColor RangeColor = FLinearColor(1.0f, 1.0f, 1.0f);

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;

	TWeakObjectPtr<AZL_Solomon_ChessPiece> Piece;
	TWeakObjectPtr<APlayerController> PC;
	float AimDistance = 5000.0f;

	void UpdateMarker();
};
