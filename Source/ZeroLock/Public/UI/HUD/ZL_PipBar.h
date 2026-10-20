// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

class UHorizontalBox;
class UProgressBar;
class UWidgetTree;

/** Deadlock-style pips shared by the stamina bar and charged ability icons: one bar per point, full ones lit, the
 * regenerating one filling up. */
namespace ZLPipBar
{
	struct FStyle
	{
		FProgressBarStyle PipStyle;
		FLinearColor FullColor;
		FLinearColor RechargingColor;
		FVector2D PipSize;
		float PipSpacing = 0.f;
	};

	/** Rebuilds Box's pips when Count changes, then fills them from Value: the whole part is full pips, the fraction
	 * is the one filling. Pips holds the created bars (keep it in a UPROPERTY). */
	ZEROLOCK_API void Refresh(UWidgetTree* WidgetTree, UHorizontalBox* Box, TArray<TObjectPtr<UProgressBar>>& Pips,
		const FStyle& Style, float Value, int32 Count);
}
