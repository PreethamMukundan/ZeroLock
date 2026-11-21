// Copyright Preetham Mukundan (C) 2026


#include "Tempest/ZL_Tempest_Vortex.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Tempest_Vortex::AZL_Tempest_Vortex()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	DomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	RootComponent = DomeMesh;
	DomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DomeMesh->SetCastShadow(false);
	DomeMesh->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		DomeMesh->SetStaticMesh(Sphere.Object);
	}
}

AZL_Tempest_Vortex* AZL_Tempest_Vortex::SpawnFor(TSubclassOf<AZL_Tempest_Vortex> VortexClass, AZeroLockCharacter* Caster, const FVector& Location,
	const UObject* DamageSource)
{
	if (!IsValid(Caster) || !Caster->HasAuthority())
	{
		return nullptr;
	}
	UWorld* World = Caster->GetWorld();
	const TSubclassOf<AZL_Tempest_Vortex> Class = VortexClass ? VortexClass : TSubclassOf<AZL_Tempest_Vortex>(AZL_Tempest_Vortex::StaticClass());

	// Over the limit: the caster's oldest domes go.
	const int32 Max = Class->GetDefaultObject<AZL_Tempest_Vortex>()->MaxPerCaster;
	if (Max > 0)
	{
		TArray<AZL_Tempest_Vortex*> Existing;
		for (TActorIterator<AZL_Tempest_Vortex> It(World); It; ++It)
		{
			if (It->GetInstigator() == Caster && !It->IsActorBeingDestroyed())
			{
				Existing.Add(*It);
			}
		}
		Existing.Sort([](const AZL_Tempest_Vortex& A, const AZL_Tempest_Vortex& B) { return A.SpawnTime < B.SpawnTime; });
		for (int32 Index = 0; Index <= Existing.Num() - Max; ++Index)
		{
			Existing[Index]->Destroy();
		}
	}

	const FTransform Transform(FRotator::ZeroRotator, Location);
	AZL_Tempest_Vortex* Vortex = World->SpawnActorDeferred<AZL_Tempest_Vortex>(Class, Transform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Vortex)
	{
		return nullptr;
	}
	Vortex->SpawnTime = World->GetTimeSeconds();
	Vortex->DamageSource = DamageSource;
	Vortex->FinishSpawning(Transform);
	return Vortex;
}

void AZL_Tempest_Vortex::BeginPlay()
{
	Super::BeginPlay();

	// Engine sphere: 100 cm across.
	DomeMesh->SetRelativeScale3D(FVector(Radius / 50.0f));
	if (UMaterialInstanceDynamic* Material = DomeMesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), DomeColor);
	}

	if (!HasAuthority())
	{
		return;
	}
	SetLifeSpan(FMath::Max(Duration, 0.1f));
	GetWorldTimerManager().SetTimer(PulseTimer, this, &AZL_Tempest_Vortex::Pulse, FMath::Max(Interval, 0.1f), true, 0.01f);
}

void AZL_Tempest_Vortex::Pulse()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetInstigator());
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC)
	{
		Destroy();
		return;
	}

	const FVector Center = GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_Tempest_Vortex), false, Hero);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(Radius), QueryParams);

	TSet<AZeroLockCharacter*> Hit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || Hit.Contains(Villan))
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (!VillanASC || HeroASC->IsUntouchable(VillanASC))
		{
			continue;
		}
		Hit.Add(Villan);

		HeroASC->ApplySpiritDamage(VillanASC, TickDamage, DamageSource.IsValid() ? DamageSource.Get() : this);
		if (Villan->bIsDead || SlowPercent <= 0.0f || SlowDuration <= 0.0f)
		{
			continue;
		}
		const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(UZL_GE_ItemStatsTimed::StaticClass(), 1.0f, HeroASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			FZL_ItemStatValue Slow;
			Slow.Stat = EZL_ItemStat::MoveSlowPercent;
			Slow.Value = -SlowPercent;
			Spec.Data->SetDuration(SlowDuration, true);
			UZL_GE_ItemStats::SetMagnitudes(*Spec.Data, { Slow });
			HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, VillanASC);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 12, Hit.Num() > 0 ? FColor::Red : FColor::Cyan, false, Interval);
	}
#endif
}
