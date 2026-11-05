// Copyright Preetham Mukundan (C) 2026

#include "UI/HUD/ZL_HUD_Minimap.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GAS/BaseCharAttributeSet.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/HUD/ZL_MinimapVolume.h"
#include "UI/HUD/ZL_PlayerInfoBox.h"
#include "Zero_BasePlayerState.h"

namespace ZLMinimapHUD
{
	static constexpr float VolumeSearchInterval = 1.f;
	static constexpr int32 ViewConeSegments = 10;

	static FSlateBrush MakeRoundBrush(ESlateBrushRoundingType::Type Rounding, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = Rounding;
		Brush.OutlineSettings.Width = 0.f;
		return Brush;
	}

	/** No attributes yet counts as alive: a pawn that just arrived shouldn't blink off the map. */
	static bool IsAlive(const APawn* Pawn)
	{
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!ASC)
		{
			return true;
		}
		bool bFound = false;
		const float Health = ASC->GetGameplayAttributeValue(UBaseCharAttributeSet::GetCurrentHealthAttribute(), bFound);
		return !bFound || Health > 0.f;
	}

	/** Screen direction of a world yaw on a map with +X up and +Y right. */
	static FVector2f YawToScreen(float YawDegrees, bool bFlip)
	{
		float Sin = 0.f;
		float Cos = 0.f;
		FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(YawDegrees));
		const FVector2f Direction(Sin, -Cos);
		return bFlip ? -Direction : Direction;
	}
}

UZL_HUD_Minimap::UZL_HUD_Minimap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace ZLMinimapHUD;
	CircleBrush = MakeRoundBrush(ESlateBrushRoundingType::HalfHeightRadius, 0.f);
	PanelBrush = MakeRoundBrush(ESlateBrushRoundingType::FixedRadius, PanelCornerRadius);
}

void UZL_HUD_Minimap::NativeConstruct()
{
	Super::NativeConstruct();

	PanelBrush = ZLMinimapHUD::MakeRoundBrush(ESlateBrushRoundingType::FixedRadius, PanelCornerRadius);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	// Everything on it moves every frame, and the paint isn't driven by child widgets.
	ForceVolatile(true);
}

void UZL_HUD_Minimap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Volume.IsValid())
	{
		NextVolumeSearch -= InDeltaTime;
		if (NextVolumeSearch <= 0.f)
		{
			// Volumes in a sublevel can show up after the HUD does.
			NextVolumeSearch = ZLMinimapHUD::VolumeSearchInterval;
			Volume = AZL_MinimapVolume::Find(GetWorld());
		}
	}

	RefreshMarkers(InDeltaTime);

	if (const AZL_MinimapVolume* Map = Volume.Get())
	{
		UTexture2D* Texture = Map->GetMapTexture();
		if (Texture && MapBrush.GetResourceObject() != Texture)
		{
			MapBrush.SetResourceObject(Texture);
			MapBrush.ImageSize = FVector2f(Texture->GetSizeX(), Texture->GetSizeY());
			MapBrush.DrawAs = ESlateBrushDrawType::Image;
			MapBrush.Tiling = ESlateBrushTileType::NoTile;
		}
	}
}

void UZL_HUD_Minimap::RefreshMarkers(float DeltaTime)
{
	Markers.Reset();

	const AZL_MinimapVolume* Map = Volume.Get();
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const APlayerController* PC = GetOwningPlayer();
	if (!Map || !GameState || !PC)
	{
		return;
	}

	const AZero_BasePlayerState* LocalPlayer = Cast<AZero_BasePlayerState>(PC->PlayerState);
	const ETeamID LocalTeam = LocalPlayer ? LocalPlayer->TeamID : ETeamID::TeamNull;
	bFlipMap = ShouldFlipMap(LocalPlayer);

	TArray<APawn*> Allies;
	TArray<APawn*> Enemies;
	for (APlayerState* Player : GameState->PlayerArray)
	{
		const AZero_BasePlayerState* ZeroPlayer = Cast<AZero_BasePlayerState>(Player);
		APawn* Pawn = ZeroPlayer ? ZeroPlayer->GetPawn() : nullptr;
		if (!Pawn || !ZLMinimapHUD::IsAlive(Pawn))
		{
			continue;
		}
		// Without a team (spectating, or teams not replicated yet) nobody is hostile.
		const bool bEnemy = LocalTeam != ETeamID::TeamNull && ZeroPlayer->TeamID != ETeamID::TeamNull && ZeroPlayer->TeamID != LocalTeam;
		(bEnemy ? Enemies : Allies).Add(Pawn);
	}

	VisionCountdown -= DeltaTime;
	if (bHideUnseenEnemies && VisionCountdown <= 0.f)
	{
		VisionCountdown = VisionInterval;
		RefreshVision(Allies, Enemies);
	}

	auto AddMarker = [this, Map, PC](APawn* Pawn, bool bAlly)
	{
		if (!Map->ContainsWorld(Pawn->GetActorLocation()))
		{
			return;
		}
		FMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.MapPosition = Map->WorldToMap(Pawn->GetActorLocation());
		Marker.HeroClass = Pawn->GetClass();
		Marker.bAlly = bAlly;
		Marker.bLocal = Pawn == PC->GetPawn();
		Marker.Yaw = Marker.bLocal ? PC->GetControlRotation().Yaw : Pawn->GetActorRotation().Yaw;
	};

	// Draw order: enemies under allies, the local player on top.
	const double Now = World->GetTimeSeconds();
	for (APawn* Enemy : Enemies)
	{
		const double* Seen = LastSeenTime.Find(Enemy);
		if (!bHideUnseenEnemies || (Seen && Now - *Seen <= RevealLinger))
		{
			AddMarker(Enemy, false);
		}
	}
	for (APawn* Ally : Allies)
	{
		AddMarker(Ally, true);
	}
	Markers.StableSort([](const FMarker& A, const FMarker& B) { return !A.bLocal && B.bLocal; });

	for (const FMarker& Marker : Markers)
	{
		GetHeroIcon(Marker.HeroClass);
	}
}

void UZL_HUD_Minimap::RefreshVision(const TArray<APawn*>& Allies, const TArray<APawn*>& Enemies)
{
	const UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLMinimapVision), false);
	const double RangeSquared = FMath::Square(static_cast<double>(VisionRange));

	for (APawn* Enemy : Enemies)
	{
		const FVector Target = Enemy->GetPawnViewLocation();
		for (const APawn* Ally : Allies)
		{
			const FVector Eye = Ally->GetPawnViewLocation();
			if (FVector::DistSquared(Eye, Target) <= RangeSquared && !World->LineTraceTestByObjectType(Eye, Target, Objects, Params))
			{
				LastSeenTime.Add(Enemy, Now);
				break;
			}
		}
	}

	for (auto It = LastSeenTime.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

bool UZL_HUD_Minimap::ShouldFlipMap(const AZero_BasePlayerState* LocalPlayer) const
{
	const AZL_MinimapVolume* Map = Volume.Get();
	if (!bKeepOwnBaseAtBottom || !Map || !LocalPlayer)
	{
		return false;
	}
	for (const FZL_MinimapBase& Base : Map->GetBases())
	{
		if (Base.Team == LocalPlayer->TeamID)
		{
			return Base.Position.Y < 0.5f;
		}
	}
	return false;
}

const FZL_MinimapHeroIcon& UZL_HUD_Minimap::GetHeroIcon(UClass* HeroClass)
{
	if (const FZL_MinimapHeroIcon* Found = HeroIcons.Find(HeroClass))
	{
		return *Found;
	}

	const UZL_Character_Data_Asset* Hero = UZL_PlayerInfoBox::FindHeroData(this, HeroClass);
	UTexture2D* Icon = (Hero && Hero->Icon) ? Hero->Icon.Get() : DefaultHeroIcon.Get();

	FZL_MinimapHeroIcon Entry;
	Entry.Brush = ZLMinimapHUD::MakeRoundBrush(ESlateBrushRoundingType::HalfHeightRadius, 0.f);
	Entry.Brush.SetResourceObject(Icon);
	Entry.bHasIcon = Icon != nullptr;
	Entry.Fill = Hero ? FLinearColor(Hero->IconColor) * 0.55f : FLinearColor(0.06f, 0.06f, 0.07f, 1.f);
	Entry.Fill.A = 1.f;
	return HeroIcons.Add(HeroClass, Entry);
}

int32 UZL_HUD_Minimap::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace ZLMinimapHUD;

	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const AZL_MinimapVolume* Map = Volume.Get();
	if (!Map && !IsDesignTime())
	{
		return LayerId;
	}

	const FVector2f WidgetSize = FVector2f(AllottedGeometry.GetLocalSize());
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	auto Faded = [Opacity](FLinearColor Color)
	{
		Color.A *= Opacity;
		return Color;
	};
	auto DrawBox = [&](int32 Layer, const FVector2f& Center, const FVector2f& Size, const FSlateBrush* Brush, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5f)),
			Brush, ESlateDrawEffect::None, Faded(Color));
	};

	DrawBox(++LayerId, WidgetSize * 0.5f, WidgetSize, &PanelBrush, PanelColor);
	if (!Map)
	{
		return LayerId;
	}

	// Largest rect of the map's shape that fits inside the padding.
	const FVector2f Available = FVector2f(
		FMath::Max(1.f, WidgetSize.X - MapPadding * 2.f),
		FMath::Max(1.f, WidgetSize.Y - MapPadding * 2.f));
	const float Aspect = Map->GetMapAspect();
	const FVector2f MapSize = Available.X / Available.Y > Aspect
		? FVector2f(Available.Y * Aspect, Available.Y)
		: FVector2f(Available.X, Available.X / Aspect);
	const FVector2f MapOrigin = (WidgetSize - MapSize) * 0.5f;
	auto ToWidget = [this, &MapOrigin, &MapSize](const FVector2f& MapPosition)
	{
		const FVector2f Oriented = bFlipMap ? FVector2f(1.f, 1.f) - MapPosition : MapPosition;
		return MapOrigin + Oriented * MapSize;
	};

	if (MapBrush.GetResourceObject())
	{
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, ++LayerId,
			AllottedGeometry.ToPaintGeometry(MapSize, FSlateLayoutTransform(MapOrigin)),
			&MapBrush, ESlateDrawEffect::None, bFlipMap ? UE_PI : 0.f, TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement, Faded(FLinearColor::White));
	}

	++LayerId;
	for (const FZL_MinimapLane& Lane : Map->GetLanes())
	{
		TArray<FVector2f> Points;
		Points.Reserve(Lane.Points.Num());
		for (const FVector2f& Point : Lane.Points)
		{
			Points.Add(ToWidget(Point));
		}
		FLinearColor Color = Lane.Color;
		Color.A *= LaneOpacity;
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
			ESlateDrawEffect::None, Faded(Color), true, LaneThickness);
	}

	const AZero_BasePlayerState* LocalPlayer = GetOwningPlayer() ? Cast<AZero_BasePlayerState>(GetOwningPlayer()->PlayerState) : nullptr;
	const ETeamID LocalTeam = LocalPlayer ? LocalPlayer->TeamID : ETeamID::TeamNull;
	++LayerId;
	for (const FZL_MinimapBase& Base : Map->GetBases())
	{
		const FLinearColor Color = (LocalTeam == ETeamID::TeamNull || Base.Team == LocalTeam) ? AllyColor : EnemyColor;
		const FVector2f Size(BaseMarkerSize, BaseMarkerSize);
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(ToWidget(Base.Position) - Size * 0.5f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, UE_PI * 0.25f, TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement, Faded(Color));
	}

	for (const FMarker& Marker : Markers)
	{
		if (!Marker.bLocal || ViewConeLength <= 0.f)
		{
			continue;
		}

		// Fan of triangles from the hero, fading out toward the far edge.
		const FVector2f Center = ToWidget(Marker.MapPosition);
		const FSlateRenderTransform Transform = AllottedGeometry.GetAccumulatedRenderTransform();
		FLinearColor TipColor = Faded(ViewConeColor);
		FLinearColor FarColor = TipColor;
		FarColor.A = 0.f;

		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Center, FVector2f(0.5f, 0.5f), TipColor.ToFColor(true)));
		for (int32 Index = 0; Index <= ViewConeSegments; ++Index)
		{
			const float Yaw = Marker.Yaw - ViewConeAngle * 0.5f + ViewConeAngle * Index / ViewConeSegments;
			const FVector2f Point = Center + YawToScreen(Yaw, bFlipMap) * ViewConeLength;
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Point, FVector2f(0.5f, 0.5f), FarColor.ToFColor(true)));
			if (Index > 0)
			{
				Indices.Append({ SlateIndex(0), SlateIndex(Index), SlateIndex(Index + 1) });
			}
		}
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++LayerId, Handle, Vertices, Indices, nullptr, 0, 0);
	}

	for (const FMarker& Marker : Markers)
	{
		const FZL_MinimapHeroIcon* Icon = HeroIcons.Find(Marker.HeroClass);
		const FVector2f Center = ToWidget(Marker.MapPosition);
		const float Diameter = Marker.bLocal ? LocalHeroIconSize : HeroIconSize;
		const FVector2f Inner(Diameter - RingThickness * 2.f, Diameter - RingThickness * 2.f);
		const FLinearColor Ring = Marker.bLocal ? LocalColor : (Marker.bAlly ? AllyColor : EnemyColor);

		DrawBox(++LayerId, Center, FVector2f(Diameter, Diameter), &CircleBrush, Ring);
		DrawBox(++LayerId, Center, Inner, &CircleBrush, Icon ? Icon->Fill : FLinearColor::Black);
		if (Icon && Icon->bHasIcon)
		{
			DrawBox(++LayerId, Center, Inner, &Icon->Brush, FLinearColor::White);
		}
	}

	return LayerId;
}
