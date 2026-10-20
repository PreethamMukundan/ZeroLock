// Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_PipBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"

void ZLPipBar::Refresh(UWidgetTree* WidgetTree, UHorizontalBox* Box, TArray<TObjectPtr<UProgressBar>>& Pips,
	const FStyle& Style, float Value, int32 Count)
{
	if (!Box || !WidgetTree)
	{
		return;
	}

	Count = FMath::Max(0, Count);
	if (Count != Pips.Num())
	{
		Box->ClearChildren();
		Pips.Reset();

		for (int32 i = 0; i < Count; ++i)
		{
			USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			SizeBox->SetWidthOverride(Style.PipSize.X);
			SizeBox->SetHeightOverride(Style.PipSize.Y);

			UProgressBar* Pip = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
			Pip->SetWidgetStyle(Style.PipStyle);
			SizeBox->AddChild(Pip);

			if (UHorizontalBoxSlot* PipSlot = Box->AddChildToHorizontalBox(SizeBox))
			{
				PipSlot->SetPadding(FMargin(i == 0 ? 0.0f : Style.PipSpacing, 0.0f, 0.0f, 0.0f));
				PipSlot->SetVerticalAlignment(VAlign_Center);
			}
			Pips.Add(Pip);
		}
	}

	const int32 FullPips = FMath::FloorToInt(Value + KINDA_SMALL_NUMBER);
	for (int32 i = 0; i < Pips.Num(); ++i)
	{
		UProgressBar* Pip = Pips[i];
		if (i < FullPips)
		{
			Pip->SetPercent(1.0f);
			Pip->SetFillColorAndOpacity(Style.FullColor);
		}
		else if (i == FullPips)
		{
			Pip->SetPercent(FMath::Frac(Value));
			Pip->SetFillColorAndOpacity(Style.RechargingColor);
		}
		else
		{
			Pip->SetPercent(0.0f);
		}
	}
}
