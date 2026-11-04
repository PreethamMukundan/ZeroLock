//Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_HUD_CrosshairBase.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UZL_HUD_CrosshairBase::AddToDelegate()
{
}

int32 UZL_HUD_CrosshairBase::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bDrawCrosshair)
	{
		return LayerId;
	}

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const float Tint = InWidgetStyle.GetColorAndOpacityTint().A;

	// Work in screen pixels so lines stay crisp (and even) at any DPI scale.
	const float Scale = FMath::Max(AllottedGeometry.GetAccumulatedLayoutTransform().GetScale(), KINDA_SMALL_NUMBER);
	auto Px = [Scale](float Units, float Min) { return FMath::Max(Min, FMath::RoundToFloat(Units * Scale)); };
	const float T = Px(LineThickness, 1.f);
	const float L = Px(LineLength, 0.f);
	const float G = Px(Gap, 0.f);
	const float D = Px(CenterDotSize, 1.f);
	const float O = Px(OutlineThickness, 0.f);
	const FVector2f Center(
		FMath::RoundToFloat(AllottedGeometry.GetLocalSize().X * 0.5f * Scale),
		FMath::RoundToFloat(AllottedGeometry.GetLocalSize().Y * 0.5f * Scale));

	struct FRect { FVector2f Pos; FVector2f Size; };
	TArray<FRect, TInlineAllocator<5>> Rects;
	if (L > 0.f)
	{
		// Odd thickness with even centre: keep the extra pixel on the same side for all lines.
		const float HalfT = FMath::FloorToFloat(T * 0.5f);
		Rects.Add({ FVector2f(Center.X + G, Center.Y - HalfT), FVector2f(L, T) });           // right
		Rects.Add({ FVector2f(Center.X - G - L, Center.Y - HalfT), FVector2f(L, T) });       // left
		Rects.Add({ FVector2f(Center.X - HalfT, Center.Y + G), FVector2f(T, L) });           // bottom
		if (bShowTopLine)
		{
			Rects.Add({ FVector2f(Center.X - HalfT, Center.Y - G - L), FVector2f(T, L) });   // top
		}
	}
	if (bShowCenterDot)
	{
		const float HalfD = FMath::FloorToFloat(D * 0.5f);
		Rects.Add({ FVector2f(Center.X - HalfD, Center.Y - HalfD), FVector2f(D, D) });
	}

	auto Draw = [&](const FRect& R, float Grow, const FLinearColor& InColor, int32 Layer)
	{
		const FVector2f Pos = (R.Pos - FVector2f(Grow)) / Scale;
		const FVector2f Size = (R.Size + FVector2f(Grow * 2.f)) / Scale;
		FLinearColor C = InColor;
		C.A *= Tint;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2D(Size), FSlateLayoutTransform(FVector2D(Pos))),
			Brush, ESlateDrawEffect::None, C);
	};

	if (O > 0.f && OutlineColor.A > 0.f)
	{
		++LayerId;
		for (const FRect& R : Rects)
		{
			Draw(R, O, OutlineColor, LayerId);
		}
	}
	++LayerId;
	for (const FRect& R : Rects)
	{
		Draw(R, 0.f, Color, LayerId);
	}
	return LayerId;
}
