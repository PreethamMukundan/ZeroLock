// Copyright Preetham Mukundan (C) 2026

#include "UI/HUD/ZL_MinimapVolume.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gamemode/Zero_BasePlayerStart.h"
#include "HAL/PlatformTime.h"
#include "Movement/Zero_ZiplineActor.h"

namespace ZLMinimap
{
	static constexpr float NoHit = -FLT_MAX;
	/** Checking the clock costs more than a trace or two, so only look every so often. */
	static constexpr int32 TracesPerClockCheck = 64;
}

AZL_MinimapVolume::AZL_MinimapVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	MapBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("MapBounds"));
	MapBounds->SetBoxExtent(FVector(5000.f, 5000.f, 3000.f));
	MapBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MapBounds->SetCanEverAffectNavigation(false);
	MapBounds->SetLineThickness(20.f);
	RootComponent = MapBounds;

	LaneColors = {
		FLinearColor(1.00f, 0.72f, 0.14f, 1.f), // yellow
		FLinearColor(0.20f, 0.85f, 0.50f, 1.f), // green
		FLinearColor(0.68f, 0.38f, 1.00f, 1.f), // purple
	};
}

AZL_MinimapVolume* AZL_MinimapVolume::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<AZL_MinimapVolume> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void AZL_MinimapVolume::BeginPlay()
{
	Super::BeginPlay();

	// Nothing draws on a dedicated server.
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(false);
	}
}

UTexture2D* AZL_MinimapVolume::GetMapTexture() const
{
	return MapTextureOverride ? MapTextureOverride.Get() : BakedTexture.Get();
}

FVector2f AZL_MinimapVolume::WorldToMap(const FVector& WorldLocation) const
{
	const FVector Min = MapBounds->GetComponentLocation() - MapBounds->GetScaledBoxExtent();
	const FVector Size = MapBounds->GetScaledBoxExtent() * 2.0;
	return FVector2f(
		static_cast<float>((WorldLocation.Y - Min.Y) / Size.Y),
		static_cast<float>(1.0 - (WorldLocation.X - Min.X) / Size.X));
}

bool AZL_MinimapVolume::ContainsWorld(const FVector& WorldLocation) const
{
	const FVector Local = (WorldLocation - MapBounds->GetComponentLocation()).GetAbs();
	const FVector Extent = MapBounds->GetScaledBoxExtent();
	return Local.X <= Extent.X && Local.Y <= Extent.Y && Local.Z <= Extent.Z;
}

float AZL_MinimapVolume::GetMapAspect() const
{
	const FVector Extent = MapBounds->GetScaledBoxExtent();
	return Extent.X > 0.0 ? static_cast<float>(Extent.Y / Extent.X) : 1.f;
}

bool AZL_MinimapVolume::AreLevelsReady() const
{
	const UWorld* World = GetWorld();
	if (!World || World->IsVisibilityRequestPending())
	{
		return false;
	}
	// The geometry can live in a sublevel that a joining client hasn't finished streaming.
	for (const ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (Level && Level->ShouldBeLoaded() && Level->ShouldBeVisible() && !Level->IsLevelVisible())
		{
			return false;
		}
	}
	return true;
}

void AZL_MinimapVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!AreLevelsReady())
	{
		return;
	}
	if (!bCollected)
	{
		bCollected = true;
		CollectFeatures();
	}
	if (MapTextureOverride)
	{
		SetActorTickEnabled(false);
		return;
	}
	BakeRows();
}

void AZL_MinimapVolume::CollectFeatures()
{
	struct FSortableLane
	{
		FZL_MinimapLane Lane;
		double MeanY = 0.0;
	};
	TArray<FSortableLane> Found;

	for (TActorIterator<AZero_ZiplineActor> It(GetWorld()); It; ++It)
	{
		const USplineComponent* Spline = It->GetZiplineComponent();
		const float Length = Spline ? Spline->GetSplineLength() : 0.f;
		if (Length <= 0.f)
		{
			continue;
		}

		FSortableLane& Entry = Found.AddDefaulted_GetRef();
		const int32 Segments = FMath::Max(1, FMath::CeilToInt(Length / LaneSampleStep));
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const FVector Point = Spline->GetLocationAtDistanceAlongSpline(Length * Index / Segments, ESplineCoordinateSpace::World);
			Entry.Lane.Points.Add(WorldToMap(Point));
			Entry.MeanY += Point.Y / (Segments + 1);
		}
	}

	Found.Sort([](const FSortableLane& A, const FSortableLane& B) { return A.MeanY > B.MeanY; });
	Lanes.Reset(Found.Num());
	for (int32 Index = 0; Index < Found.Num(); ++Index)
	{
		if (LaneColors.Num() > 0)
		{
			Found[Index].Lane.Color = LaneColors[Index % LaneColors.Num()];
		}
		Lanes.Add(MoveTemp(Found[Index].Lane));
	}

	TMap<ETeamID, TPair<FVector, int32>> Starts;
	for (TActorIterator<AZero_BasePlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->TeamID == ETeamID::TeamNull)
		{
			continue;
		}
		TPair<FVector, int32>& Sum = Starts.FindOrAdd(It->TeamID, TPair<FVector, int32>(FVector::ZeroVector, 0));
		Sum.Key += It->GetActorLocation();
		++Sum.Value;
	}
	Bases.Reset(Starts.Num());
	for (const TPair<ETeamID, TPair<FVector, int32>>& Start : Starts)
	{
		FZL_MinimapBase& Base = Bases.AddDefaulted_GetRef();
		Base.Team = Start.Key;
		Base.Position = WorldToMap(Start.Value.Key / Start.Value.Value);
	}
}

void AZL_MinimapVolume::BakeRows()
{
	const FVector Center = MapBounds->GetComponentLocation();
	const FVector Extent = MapBounds->GetScaledBoxExtent();

	if (Heights.IsEmpty())
	{
		const float Aspect = GetMapAspect();
		BakeWidth = Aspect >= 1.f ? Resolution : FMath::Max(1, FMath::RoundToInt(Resolution * Aspect));
		BakeHeight = Aspect >= 1.f ? FMath::Max(1, FMath::RoundToInt(Resolution / Aspect)) : Resolution;
		Heights.Init(ZLMinimap::NoHit, BakeWidth * BakeHeight);
		NextPixel = 0;
	}

	const UWorld* World = GetWorld();
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLMinimapBake), true);
	Params.AddIgnoredActor(this);

	const double Deadline = FPlatformTime::Seconds() + BakeBudgetMs * 0.001;
	int32 SinceClockCheck = 0;
	while (NextPixel < Heights.Num())
	{
		const int32 Column = NextPixel % BakeWidth;
		const int32 Row = NextPixel / BakeWidth;
		const double X = Center.X + Extent.X - (Row + 0.5) / BakeHeight * Extent.X * 2.0;
		const double Y = Center.Y - Extent.Y + (Column + 0.5) / BakeWidth * Extent.Y * 2.0;

		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, FVector(X, Y, Center.Z + Extent.Z), FVector(X, Y, Center.Z - Extent.Z), Objects, Params))
		{
			Heights[NextPixel] = static_cast<float>(Hit.ImpactPoint.Z);
		}
		++NextPixel;

		if (++SinceClockCheck >= ZLMinimap::TracesPerClockCheck)
		{
			SinceClockCheck = 0;
			if (FPlatformTime::Seconds() >= Deadline)
			{
				return;
			}
		}
	}

	FinishBake();
}

void AZL_MinimapVolume::FinishBake()
{
	using namespace ZLMinimap;

	auto HeightAt = [this](int32 Column, int32 Row)
	{
		if (Column < 0 || Row < 0 || Column >= BakeWidth || Row >= BakeHeight)
		{
			return NoHit;
		}
		return Heights[Row * BakeWidth + Column];
	};

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Heights.Num());
	for (int32 Row = 0; Row < BakeHeight; ++Row)
	{
		for (int32 Column = 0; Column < BakeWidth; ++Column)
		{
			const float Height = HeightAt(Column, Row);
			FColor& Pixel = Pixels[Row * BakeWidth + Column];
			if (Height == NoHit)
			{
				Pixel = FColor(0, 0, 0, 0);
				continue;
			}

			const float Relative = Height - GroundZ;
			FLinearColor Color = StreetColor;
			if (Relative < -PitDepth)
			{
				Color = PitColor;
			}
			else if (Relative > StreetMaxHeight)
			{
				Color = FLinearColor::LerpUsingHSV(BlockLowColor, BlockHighColor, FMath::Clamp(Relative / BlockMaxHeight, 0.f, 1.f));
			}

			bool bEdge = false;
			for (const FIntPoint& Offset : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
			{
				const float Neighbour = HeightAt(Column + Offset.X, Row + Offset.Y);
				bEdge |= Neighbour == NoHit || FMath::Abs(Neighbour - Height) >= EdgeHeight;
			}
			Pixel = (bEdge ? EdgeColor : Color).ToFColor(true);
			Pixel.A = 255;
		}
	}

	Heights.Empty();
	SetActorTickEnabled(false);

	UTexture2D* Texture = UTexture2D::CreateTransient(BakeWidth, BakeHeight, PF_B8G8R8A8, MakeUniqueObjectName(this, UTexture2D::StaticClass(), TEXT("T_MinimapBake")));
	if (!Texture)
	{
		return;
	}
	Texture->Filter = TF_Bilinear;
	Texture->LODGroup = TEXTUREGROUP_UI;
	Texture->NeverStream = true;
	Texture->SRGB = true;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	BakedTexture = Texture;
}
