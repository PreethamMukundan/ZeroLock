// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_Bishop.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_GE_MoveSlow.h"
#include "Solomon/ZL_Solomon_BishopSquare.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_Solomon_Bishop::AZL_Solomon_Bishop()
{
	// Deadlock: MoveDistance 12 m, BishopMoveSpeed 1500 u/s, MinBishopMoveDuration 0.15, MoveCooldown 3 (1 m = 70 cm, 1 u = 1.778 cm).
	MoveDistance = 840.0f;
	MoveSpeed = 2667.0f;
	MinMoveDuration = 0.15f;
	MoveCooldown = 3.0f;

	// Placeholder: a tall engine Cone, pivot on the floor.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		PieceMesh->SetStaticMesh(ConeMesh.Object);
	}
	PieceMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));
	PieceMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));

	SlowEffect = UZL_GE_MoveSlow::StaticClass();
	SquareClass = AZL_Solomon_BishopSquare::StaticClass();
}

void AZL_Solomon_Bishop::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bSquaresOnPlace)
	{
		GetWorldTimerManager().SetTimer(SquareTimer, this, &AZL_Solomon_Bishop::SpawnSquares, FMath::Max(MoveCooldown, 0.01f));
	}
}

void AZL_Solomon_Bishop::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(SquareTimer);
		GetWorldTimerManager().ClearTimer(SlowTimer);
		ClearSquares();
		for (const TPair<TWeakObjectPtr<AZeroLockCharacter>, FFadingSlow>& Pair : FadingSlows)
		{
			RemoveSlow(Pair.Key.Get(), Pair.Value.Handle);
		}
		FadingSlows.Empty();
	}
	Super::EndPlay(EndPlayReason);
}

void AZL_Solomon_Bishop::OnMoveStarted()
{
	HitThisMove.Reset();
	GetWorldTimerManager().ClearTimer(SquareTimer);
	ClearSquares();
}

void AZL_Solomon_Bishop::OnMoveStep(const FVector& From, const FVector& To)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}
	// Capsule along this tick's segment, standing on the path (pieces slide on the floor, heroes stand on it).
	const float HalfHeight = 100.0f;
	const FVector Up(0.0f, 0.0f, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BishopPass), false, this);
	Params.AddIgnoredActor(Hero);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, From + Up, To + Up + FVector(0.0f, 0.0f, 0.01f), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeCapsule(PassRadius, HalfHeight + PassRadius), Params);
	for (const FHitResult& Hit : Hits)
	{
		HitVillan(Cast<AZeroLockCharacter>(Hit.GetActor()));
	}
}

void AZL_Solomon_Bishop::OnMoveFinished()
{
	GetWorldTimerManager().SetTimer(SquareTimer, this, &AZL_Solomon_Bishop::SpawnSquares, FMath::Max(MoveCooldown, 0.01f));
}

void AZL_Solomon_Bishop::HitVillan(AZeroLockCharacter* Villan)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || HitThisMove.Contains(Villan))
	{
		return;
	}
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC)
	{
		return;
	}
	HitThisMove.Add(Villan);
	HeroASC->ApplySpiritDamage(VillanASC, PassDamage, this);

	if (!SlowEffect || SlowPercent <= 0.0f || SlowDuration <= 0.0f)
	{
		return;
	}
	// A new hit restarts the fade at full strength.
	FFadingSlow& Slow = FadingSlows.FindOrAdd(Villan);
	RemoveSlow(Villan, Slow.Handle);
	Slow.Handle = ApplySlow(Villan, 1.0f - SlowPercent / 100.0f);
	Slow.StartTime = GetWorld()->GetTimeSeconds();
	if (!GetWorldTimerManager().IsTimerActive(SlowTimer))
	{
		GetWorldTimerManager().SetTimer(SlowTimer, this, &AZL_Solomon_Bishop::TickSlows, FMath::Max(SlowTickInterval, 0.05f), true);
	}
}

void AZL_Solomon_Bishop::TickSlows()
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (auto It = FadingSlows.CreateIterator(); It; ++It)
	{
		AZeroLockCharacter* Villan = It->Key.Get();
		FFadingSlow& Slow = It->Value;
		const float Remaining = 1.0f - (Now - Slow.StartTime) / SlowDuration;
		RemoveSlow(Villan, Slow.Handle);
		if (!IsValid(Villan) || Villan->bIsDead || Remaining <= 0.0f)
		{
			It.RemoveCurrent();
			continue;
		}
		Slow.Handle = ApplySlow(Villan, 1.0f - SlowPercent / 100.0f * Remaining);
	}
	if (FadingSlows.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SlowTimer);
	}
}

FActiveGameplayEffectHandle AZL_Solomon_Bishop::ApplySlow(AZeroLockCharacter* Villan, float Multiplier)
{
	AZeroLockCharacter* Hero = GetHero();
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	UBaseCharAbilitySystemComponent* VillanASC = Villan ? Villan->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC || !VillanASC || !SlowEffect)
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(SlowEffect, 1.0f, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_MOVE_SPEED_MULTIPLIER, Multiplier);
	Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_SLOWED);
	return HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), VillanASC);
}

void AZL_Solomon_Bishop::RemoveSlow(AZeroLockCharacter* Villan, const FActiveGameplayEffectHandle& Handle)
{
	UBaseCharAbilitySystemComponent* VillanASC = Villan ? Villan->GetMyAbilitySystemComp() : nullptr;
	if (VillanASC && Handle.IsValid())
	{
		VillanASC->RemoveActiveGameplayEffect(Handle);
	}
}

void AZL_Solomon_Bishop::SpawnSquares()
{
	ClearSquares();
	if (!SquareClass)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BishopSquares), false, this);
	if (AActor* Hero = GetInstigator())
	{
		Params.AddIgnoredActor(Hero);
	}

	const FVector Origin = GetActorLocation();
	TArray<FVector> Centers;
	if (bSquareUnderPiece)
	{
		Centers.Add(Origin);
	}
	static const FVector2D Diagonals[] = {{1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {-1.0f, -1.0f}};
	for (const FVector2D& Diagonal : Diagonals)
	{
		for (int32 Step = 1; Step <= TileSteps; ++Step)
		{
			Centers.Add(Origin + FVector(Diagonal.X, Diagonal.Y, 0.0f) * (SquareSize * Step));
		}
	}

	for (const FVector& Center : Centers)
	{
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByObjectType(Floor, Center + FVector(0.0f, 0.0f, 200.0f), Center - FVector(0.0f, 0.0f, 600.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			continue;
		}
		const FTransform Transform(FRotator::ZeroRotator, Floor.ImpactPoint + FVector(0.0f, 0.0f, SquareHeight * 0.5f));
		AZL_Solomon_BishopSquare* Square = GetWorld()->SpawnActorDeferred<AZL_Solomon_BishopSquare>(SquareClass, Transform, this, GetInstigator(),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Square)
		{
			continue;
		}
		Square->InitSquare(SquareSize, SquareHeight, SquareDamage);
		Square->FinishSpawning(Transform);
		if (IsValid(Square))
		{
			Squares.Add(Square);
		}
	}
}

void AZL_Solomon_Bishop::ClearSquares()
{
	for (const TWeakObjectPtr<AZL_Solomon_BishopSquare>& Square : Squares)
	{
		if (Square.IsValid())
		{
			Square->Destroy();
		}
	}
	Squares.Empty();
}
