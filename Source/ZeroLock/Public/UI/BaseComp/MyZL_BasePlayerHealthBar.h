//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/BaseComp/ZL_BaseProgressBar.h"
#include "MyZL_BasePlayerHealthBar.generated.h"

class UProgressBar;
class UTextBlock;
class UZL_VM_Attributes;

/**
 * Base of WBP_PlayerHealth. Adds the combat barrier on top of the BP's health bar at runtime (no BP changes): the first
 * UProgressBar in the widget tree (or HealthBarName) must sit in an Overlay; a barrier bar (same style, transparent
 * background, BarrierColor, filling from the right: barrier / max health) and a "+80" label go into that overlay above
 * it, collapsed while there's no barrier. Reads the widget's ZL_VM_Attributes viewmodel, else the owning pawn's.
 */
UCLASS()
class ZEROLOCK_API UMyZL_BasePlayerHealthBar : public UZL_BaseProgressBar
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The health bar to put the barrier on; None = the first progress bar in the tree. */
	UPROPERTY(EditAnywhere, Category = "Barrier")
	FName HealthBarName;

	UPROPERTY(EditAnywhere, Category = "Barrier")
	FLinearColor BarrierColor = FLinearColor(0.55f, 0.85f, 1.0f, 0.85f);

	UPROPERTY(EditAnywhere, Category = "Barrier")
	int32 BarrierTextSize = 14;

private:
	UZL_VM_Attributes* GetAttributes() const;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BarrierBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BarrierText;

	float LastBarrier = -1.0f;
	float LastMaxHealth = -1.0f;
};
