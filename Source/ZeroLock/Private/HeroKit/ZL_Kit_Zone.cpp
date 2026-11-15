// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_Zone.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Kit_Zone::AZL_Kit_Zone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	DiscMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiscMesh"));
	RootComponent = DiscMesh;
	DiscMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DiscMesh->SetCastShadow(false);
	DiscMesh->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		DiscMesh->SetStaticMesh(Cylinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShape(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicShape.Succeeded())
	{
		DiscMesh->SetMaterial(0, BasicShape.Object);
	}
}

void AZL_Kit_Zone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZL_Kit_Zone, VisualRadius);
	DOREPLIFETIME(AZL_Kit_Zone, VisualColor);
	DOREPLIFETIME(AZL_Kit_Zone, bFollow);
	DOREPLIFETIME_CONDITION(AZL_Kit_Zone, VisualLoopCue, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AZL_Kit_Zone, bVisualHideDisc, COND_InitialOnly);
}

FVector AZL_Kit_Zone::SnapToFloor(const UWorld* World, const FVector& Location)
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLKitZoneFloor), false);
	if (World && World->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, 50), Location - FVector(0, 0, 2000), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

AZL_Kit_Zone* AZL_Kit_Zone::Spawn(TSubclassOf<AZL_Kit_Zone> ZoneClass, AZeroLockCharacter* Caster, const FVector& Location, const FZL_KitZoneSpec& InSpec,
	float PowerScale, float RangeScale, FName Key)
{
	if (!IsValid(Caster) || !Caster->HasAuthority() || !InSpec.IsValid())
	{
		return nullptr;
	}
	UWorld* World = Caster->GetWorld();

	// Over the limit: the oldest of this caster's zones with the same key goes.
	if (InSpec.MaxPerCaster > 0)
	{
		TArray<AZL_Kit_Zone*> Existing;
		for (TActorIterator<AZL_Kit_Zone> It(World); It; ++It)
		{
			if (It->GetInstigator() == Caster && It->ZoneKey == Key && !It->IsActorBeingDestroyed())
			{
				Existing.Add(*It);
			}
		}
		Existing.Sort([](const AZL_Kit_Zone& A, const AZL_Kit_Zone& B) { return A.SpawnTime < B.SpawnTime; });
		for (int32 Index = 0; Index <= Existing.Num() - InSpec.MaxPerCaster; ++Index)
		{
			Existing[Index]->Destroy();
		}
	}

	const FTransform Transform(FRotator::ZeroRotator, InSpec.bFollowCaster ? Caster->GetActorLocation() : Location);
	AZL_Kit_Zone* Zone = World->SpawnActorDeferred<AZL_Kit_Zone>(ZoneClass ? ZoneClass.Get() : AZL_Kit_Zone::StaticClass(), Transform, Caster, Caster,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Zone)
	{
		return nullptr;
	}
	Zone->Spec = InSpec;
	Zone->Power = PowerScale;
	Zone->Radius = InSpec.Radius * RangeScale;
	Zone->ZoneKey = Key;
	Zone->SpawnTime = World->GetTimeSeconds();
	Zone->VisualRadius = Zone->Radius;
	Zone->VisualColor = InSpec.Color;
	Zone->bFollow = InSpec.bFollowCaster;
	Zone->VisualLoopCue = InSpec.LoopCue.Cue;
	Zone->bVisualHideDisc = InSpec.bHideDisc;
	Zone->FinishSpawning(Transform);
	return Zone;
}

void AZL_Kit_Zone::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Visual();
	SetActorTickEnabled(bFollow);
	ZLKitCue::AddLocal(this, VisualLoopCue, VisualRadius);
	if (bVisualHideDisc && DiscMesh)
	{
		DiscMesh->SetVisibility(false);
	}

	if (!HasAuthority())
	{
		return;
	}
	SetLifeSpan(Spec.Duration);

	FTimerManager& Timers = GetWorldTimerManager();
	if (Spec.bTriggerOnce)
	{
		Timers.SetTimer(PulseTimer, this, &AZL_Kit_Zone::CheckTrigger, 0.1f, true, FMath::Max(Spec.ArmDelay, 0.1f));
	}
	else
	{
		const float Interval = FMath::Max(Spec.Interval, 0.1f);
		Timers.SetTimer(PulseTimer, this, &AZL_Kit_Zone::Pulse, Interval, true, Spec.ArmDelay > 0.f ? Spec.ArmDelay : 0.01f);
	}
}

void AZL_Kit_Zone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ZLKitCue::RemoveLocal(this, VisualLoopCue);
	Super::EndPlay(EndPlayReason);
}

void AZL_Kit_Zone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const AActor* Caster = GetInstigator())
	{
		SetActorLocation(SnapToFloor(GetWorld(), Caster->GetActorLocation()));
	}
}

void AZL_Kit_Zone::OnRep_Visual()
{
	if (!DiscMesh)
	{
		return;
	}
	// Engine cylinder: 100 cm wide, 100 cm tall.
	DiscMesh->SetRelativeScale3D(FVector(VisualRadius / 50.f, VisualRadius / 50.f, DiscThickness / 100.f));
	if (!DiscMaterial)
	{
		DiscMaterial = DiscMesh->CreateDynamicMaterialInstance(0);
	}
	if (DiscMaterial)
	{
		DiscMaterial->SetVectorParameterValue(TEXT("Color"), VisualColor);
	}
}

AZeroLockCharacter* AZL_Kit_Zone::GetCaster() const
{
	return Cast<AZeroLockCharacter>(GetInstigator());
}

void AZL_Kit_Zone::HitArea(const FVector& Center, float HitRadius)
{
	AZeroLockCharacter* Caster = GetCaster();
	if (!Caster)
	{
		Destroy();
		return;
	}
	ZLKitCue::Play(Caster->GetMyAbilitySystemComp(), Spec.PulseCue, Center, FRotator::ZeroRotator, HitRadius);
	FZL_KitArea Area;
	Area.Shape = EZL_KitShape::Sphere;
	Area.Range = HitRadius;
	Area.bRequireLineOfSight = false;


	// Heroes stand above the disc: measure from about chest height.
	const FVector Origin = Center + FVector(0, 0, 60);
	TArray<AZeroLockCharacter*> Villans;
	if (!Spec.EnemyPayload.IsEmpty())
	{
		ZLKit::FindHeroes(Caster, Origin, FVector::ForwardVector, Area, 1.f, EZL_KitTeam::Enemies, Villans);
		for (AZeroLockCharacter* Villan : Villans)
		{
			ZLKit::ApplyPayload(this, Caster, Villan, Center, Spec.EnemyPayload, Power);
		}
	}
	if (!Spec.AllyPayload.IsEmpty())
	{
		TArray<AZeroLockCharacter*> Allies;
		ZLKit::FindHeroes(Caster, Origin, FVector::ForwardVector, Area, 1.f, Spec.bAllyPayloadOnCaster ? EZL_KitTeam::Allies : EZL_KitTeam::AlliesNotSelf, Allies);
		for (AZeroLockCharacter* Ally : Allies)
		{
			ZLKit::ApplyPayload(this, Caster, Ally, Center, Spec.AllyPayload, Power);
		}
	}
#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		ZLDebugDraw::Sphere(GetWorld(), Origin, HitRadius, Villans.IsEmpty() ? FColor::Cyan : FColor::Red, 0.4f, 12);
	}
#endif
}

void AZL_Kit_Zone::Pulse()
{
	if (Spec.StrikeRadius > 0.f)
	{
		const FVector2D Offset = FMath::RandPointInCircle(Radius);
		HitArea(SnapToFloor(GetWorld(), GetActorLocation() + FVector(Offset.X, Offset.Y, 0.f)), Spec.StrikeRadius);
	}
	else
	{
		HitArea(GetActorLocation(), Radius);
	}
}

void AZL_Kit_Zone::CheckTrigger()
{
	AZeroLockCharacter* Caster = GetCaster();
	if (!Caster)
	{
		Destroy();
		return;
	}
	FZL_KitArea Area;
	Area.Range = Spec.TriggerRadius;
	Area.bRequireLineOfSight = false;
	TArray<AZeroLockCharacter*> Villans;
	ZLKit::FindHeroes(Caster, GetActorLocation() + FVector(0, 0, 60), FVector::ForwardVector, Area, 1.f, EZL_KitTeam::Enemies, Villans);
	if (!Villans.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(PulseTimer);
		HitArea(GetActorLocation(), Radius);
		Destroy();
	}
}
