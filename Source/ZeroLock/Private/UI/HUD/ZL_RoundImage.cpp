// Copyright Preetham Mukundan (C) 2026

#include "UI/HUD/ZL_RoundImage.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"

/** SImage that paints its brush as a rounded box, which clips the texture to the shape. */
class SZL_RoundImage : public SImage
{
public:
	SLATE_BEGIN_ARGS(SZL_RoundImage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SImage::Construct(SImage::FArguments());
	}

	void SetOutline(float InThickness, const FLinearColor& InColor)
	{
		OutlineThickness = InThickness;
		OutlineColor = InColor;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetRadial(float InProgress, const FLinearColor& InColor, float InOverscan)
	{
		RadialProgress = InProgress;
		RadialColor = InColor;
		RadialOverscan = InOverscan;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FSlateBrush* Source = GetImageAttribute().Get();
		if (!Source || Source->DrawAs == ESlateBrushDrawType::NoDrawType)
		{
			return LayerId;
		}

		// A member, not a local: the draw element can outlive this call.
		PaintBrush = *Source;
		PaintBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PaintBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		PaintBrush.OutlineSettings.Width = OutlineThickness;
		PaintBrush.OutlineSettings.Color = FSlateColor(OutlineColor);

		const FLinearColor Tint(InWidgetStyle.GetColorAndOpacityTint() * GetColorAndOpacityAttribute().Get().GetColor(InWidgetStyle) * Source->GetTint(InWidgetStyle));
		const ESlateDrawEffect Effects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &PaintBrush, Effects, Tint);

		if (RadialProgress > 0.f && RadialProgress < 1.f)
		{
			LayerId = PaintRadial(AllottedGeometry, OutDrawElements, LayerId + 1, InWidgetStyle);
		}
		return LayerId;
	}

private:
	/** Triangle fan over the part of the circle from RadialProgress round to the top. */
	int32 PaintRadial(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle) const
	{
		static constexpr int32 FullCircleSegments = 72;

		const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
		const FVector2f Center = Size * 0.5f;
		const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f + RadialOverscan;
		const FSlateRenderTransform Transform = AllottedGeometry.GetAccumulatedRenderTransform();

		FLinearColor Color = RadialColor;
		Color.A *= InWidgetStyle.GetColorAndOpacityTint().A;
		const FColor VertexColor = Color.ToFColor(true);

		const float Remaining = 1.f - RadialProgress;
		const int32 Segments = FMath::Max(1, FMath::CeilToInt(FullCircleSegments * Remaining));

		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		Vertices.Reserve(Segments + 2);
		Indices.Reserve(Segments * 3);
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Center, FVector2f(0.5f, 0.5f), VertexColor));
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Turn = RadialProgress + Remaining * Index / Segments;
			float Sin = 0.f;
			float Cos = 0.f;
			FMath::SinCos(&Sin, &Cos, Turn * UE_TWO_PI);
			const FVector2f Point = Center + FVector2f(Sin, -Cos) * Radius;
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Point, FVector2f(0.5f, 0.5f), VertexColor));
			if (Index > 0)
			{
				Indices.Append({ SlateIndex(0), SlateIndex(Index), SlateIndex(Index + 1) });
			}
		}

		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Vertices, Indices, nullptr, 0, 0);
		return LayerId;
	}

	mutable FSlateBrush PaintBrush;
	float OutlineThickness = 0.f;
	FLinearColor OutlineColor = FLinearColor::Transparent;
	float RadialProgress = 1.f;
	FLinearColor RadialColor = FLinearColor::Transparent;
	float RadialOverscan = 0.f;
};

TSharedRef<SWidget> UZL_RoundImage::RebuildWidget()
{
	// UImage does everything else through MyImage, so this is the only thing that differs from a plain Image.
	MyImage = SNew(SZL_RoundImage);
	return MyImage.ToSharedRef();
}

void UZL_RoundImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (MyImage.IsValid())
	{
		const TSharedPtr<SZL_RoundImage> RoundImage = StaticCastSharedPtr<SZL_RoundImage>(MyImage);
		RoundImage->SetOutline(OutlineThickness, OutlineColor);
		RoundImage->SetRadial(RadialProgress, RadialColor, RadialOverscan);
	}
}

void UZL_RoundImage::SetRadialProgress(float InProgress)
{
	RadialProgress = FMath::Clamp(InProgress, 0.f, 1.f);
	if (MyImage.IsValid())
	{
		StaticCastSharedPtr<SZL_RoundImage>(MyImage)->SetRadial(RadialProgress, RadialColor, RadialOverscan);
	}
}
