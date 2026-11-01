// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_Queen.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_GE_MoveSlow.h"
#include "GameplayEffect.h"
#include "Solomon/ZL_Solomon_QueenTile.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace
{
	/** The queen's 8 lines, in board squares (diagonals land on the diagonal squares). */
	const FVector2D QueenLines[8] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {-1.0f, -1.0f}};
}

AZL_Solomon_Queen::AZL_Solomon_Queen()
{
	// Deadlock MoveDistance 16 m, MoveCooldown 5. Lands and leaps on the shared board, so its lines match the bishop's squares.
	MoveDistance = 1120.0f;
	MoveSpeed = 2000.0f;
	MinMoveDuration = 0.5f;
	MoveCooldown = 5.0f;
	PieceColor = FLinearColor(0.85f, 0.1f, 0.6f);

	// Placeholder: a big engine Cone, pivot on the floor.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		PieceMesh->SetStaticMesh(ConeMesh.Object);
	}
	PieceMesh->SetRelativeScale3D(FVector(1.1f, 1.1f, 2.4f));
	PieceMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));

	TileClass = AZL_Solomon_QueenTile::StaticClass();
	SlowEffect = UZL_GE_MoveSlow::StaticClass();
	static ConstructorHelpers::FClassFinder<UGameplayEffect> Stun(TEXT("/Game/ZERO/BP/BaseGameplayAbility/BPGE_Stun"));
	if (Stun.Succeeded())
	{
		StunEffect = Stun.Class;
	}
}

void AZL_Solomon_Queen::InitQueen(const FVector& InLand, float InDamage, float InStunDuration, float InSlowPercent, float InSlowDuration,
	int32 InTileSteps, int32 InMoves, TSubclassOf<UGameplayEffect> InStunEffect)
{
	Land = InLand;
	Damage = InDamage;
	StunDuration = InStunDuration;
	SlowPercent = InSlowPercent;
	SlowDuration = InSlowDuration;
	TileSteps = FMath::Max(InTileSteps, 0);
	MovesLeft = FMath::Max(InMoves, 0);
	if (InStunEffect)
	{
		StunEffect = InStunEffect;
	}
}

void AZL_Solomon_Queen::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		// Crash down from where it was spawned onto Land; the landing is the first strike.
		bDropping = true;
		StartMoveAlong({GetActorLocation(), Land}, DropDuration);
	}
}

void AZL_Solomon_Queen::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(StrikeTimer);
		GetWorldTimerManager().ClearTimer(ExpireTimer);
		TArray<TWeakObjectPtr<AZeroLockCharacter>> Slowed;
		Slows.GetKeys(Slowed);
		for (const TWeakObjectPtr<AZeroLockCharacter>& Villan : Slowed)
		{
			RemoveSlow(Villan);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AZL_Solomon_Queen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !IsActorBeingDestroyed() && ExpireDistance > 0.0f)
	{
		const AZeroLockCharacter* Hero = GetHero();
		if (Hero && FVector::DistSquared(Hero->GetActorLocation(), GetActorLocation()) > FMath::Square(ExpireDistance))
		{
			Destroy();
		}
	}
}

void AZL_Solomon_Queen::BuildMovePath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const
{
	// A leap: straight seen from above, an arc LeapHeight above the higher end.
	const float Length = FVector::Dist2D(From, To);
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(Length / FMath::Max(PathSampleSpacing, 10.0f)), 8, 64);
	const float PeakZ = FMath::Max(From.Z, To.Z) + LeapHeight;
	OutPath.Reset(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / Segments;
		FVector Point = FMath::Lerp(From, To, Alpha);
		// Up from the start to the peak and down to the end (a parabola through both ends and the peak's height).
		const float Rise = PeakZ - FMath::Lerp(From.Z, To.Z, 0.5f);
		Point.Z += 4.0f * Rise * Alpha * (1.0f - Alpha);
		OutPath.Add(Point);
	}
}

void AZL_Solomon_Queen::OnMoveStarted()
{
	GetWorldTimerManager().ClearTimer(StrikeTimer);
	GetWorldTimerManager().ClearTimer(ExpireTimer);
	if (bDropping)
	{
		return;
	}
	--MovesLeft;
	if (MovesLeft <= 0)
	{
		// Out of moves: Move Piece can't pick it any more (CanMove).
		NextMoveTime = TNumericLimits<float>::Max();
	}
}

void AZL_Solomon_Queen::OnMoveFinished()
{
	bDropping = false;
	Strike();
}

void AZL_Solomon_Queen::Strike()
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}
	StrikeOrigin = GetActorLocation();

	// Crash: everyone around the landing that the queen can see.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QueenCrash), false, this);
	Params.AddIgnoredActor(Hero);
	const float HalfHeight = FMath::Max(StrikeHeight * 0.5f, CrashRadius);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, StrikeOrigin + FVector(0.0f, 0.0f, HalfHeight), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeCapsule(CrashRadius, HalfHeight), Params);
	TSet<AZeroLockCharacter*> Crashed;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!Villan || Crashed.Contains(Villan))
		{
			continue;
		}
		FHitResult Blocker;
		if (GetWorld()->LineTraceSingleByObjectType(Blocker, StrikeOrigin + FVector(0.0f, 0.0f, 100.0f), Villan->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			continue;
		}
		Crashed.Add(Villan);
		HitVillan(Villan, false);
	}
#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugCapsule(GetWorld(), StrikeOrigin + FVector(0.0f, 0.0f, HalfHeight), HalfHeight, CrashRadius, FQuat::Identity, FColor::Magenta, false, 1.0f);
	}
#endif

	// Then the lines.
	HitByTiles.Reset();
	StrikeStep = 0;
	for (int32 Line = 0; Line < 8; ++Line)
	{
		bLineAlive[Line] = true;
		LineFloor[Line] = StrikeOrigin;
	}
	if (TileSteps > 0)
	{
		GetWorldTimerManager().SetTimer(StrikeTimer, this, &AZL_Solomon_Queen::StepTiles, FMath::Max(TimeBetweenTiles, 0.01f), true,
			FMath::Max(InitialTileDelay, 0.01f));
	}
	else
	{
		StrikeFinished();
	}
}

void AZL_Solomon_Queen::StepTiles()
{
	++StrikeStep;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(QueenTiles), false, this);
	if (AActor* Hero = GetInstigator())
	{
		Params.AddIgnoredActor(Hero);
	}

	for (int32 Line = 0; Line < 8; ++Line)
	{
		if (!bLineAlive[Line])
		{
			continue;
		}
		const FVector Previous = LineFloor[Line];
		const FVector Center = StrikeOrigin + FVector(QueenLines[Line].X, QueenLines[Line].Y, 0.0f) * (SquareSize * StrikeStep);
		FHitResult Floor;
		FHitResult Wall;
		const FVector Up(0.0f, 0.0f, 60.0f);
		if (!GetWorld()->LineTraceSingleByObjectType(Floor, FVector(Center.X, Center.Y, Previous.Z + 200.0f), FVector(Center.X, Center.Y, Previous.Z - 600.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
			|| Floor.bStartPenetrating
			|| GetWorld()->LineTraceSingleByObjectType(Wall, Previous + Up, Floor.ImpactPoint + Up, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			// A wall or no floor ends this line, like a blocked queen.
			bLineAlive[Line] = false;
			continue;
		}
		LineFloor[Line] = Floor.ImpactPoint;

		if (TileClass)
		{
			const FTransform Transform(FRotator::ZeroRotator, Floor.ImpactPoint + FVector(0.0f, 0.0f, 2.0f));
			if (AZL_Solomon_QueenTile* Tile = GetWorld()->SpawnActorDeferred<AZL_Solomon_QueenTile>(TileClass, Transform, this, GetInstigator(),
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Tile->InitTile(SquareSize);
				Tile->FinishSpawning(Transform);
			}
		}

		const FVector BoxCenter = Floor.ImpactPoint + FVector(0.0f, 0.0f, StrikeHeight * 0.5f);
		const FVector Extent(SquareSize * 0.5f, SquareSize * 0.5f, StrikeHeight * 0.5f);
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByObjectType(Overlaps, BoxCenter, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
			FCollisionShape::MakeBox(Extent), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
			if (Villan && !HitByTiles.Contains(Villan))
			{
				HitVillan(Villan, true);
			}
		}
#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			DrawDebugBox(GetWorld(), BoxCenter, Extent, FColor::Magenta, false, 0.5f);
		}
#endif
	}

	bool bAnyAlive = false;
	for (const bool bAlive : bLineAlive)
	{
		bAnyAlive |= bAlive;
	}
	if (StrikeStep >= TileSteps || !bAnyAlive)
	{
		GetWorldTimerManager().ClearTimer(StrikeTimer);
		StrikeFinished();
	}
}

void AZL_Solomon_Queen::StrikeFinished()
{
	if (MovesLeft <= 0)
	{
		GetWorldTimerManager().SetTimer(ExpireTimer, this, &AZL_Solomon_Queen::Expire, FMath::Max(LingerAfterLastStrike, 0.01f));
	}
}

void AZL_Solomon_Queen::Expire()
{
	Destroy();
}

void AZL_Solomon_Queen::HitVillan(AZeroLockCharacter* Villan, bool bStun)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!IsValid(Hero) || !IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan))
	{
		return;
	}
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !VillanASC || HeroASC->IsUntouchable(VillanASC))
	{
		return;
	}
	if (bStun)
	{
		HitByTiles.Add(Villan);
	}
	HeroASC->ApplySpiritDamage(VillanASC, Damage, this);
	ApplySlow(Villan);

	if (bStun && StunEffect && StunDuration > 0.0f)
	{
		FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(StunEffect, 1.0f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetDuration(StunDuration, true);
			HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), VillanASC);
		}
	}
}

void AZL_Solomon_Queen::ApplySlow(AZeroLockCharacter* Villan)
{
	AZeroLockCharacter* Hero = GetHero();
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	UBaseCharAbilitySystemComponent* VillanASC = Villan ? Villan->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC || !VillanASC || !SlowEffect || SlowPercent <= 0.0f || SlowDuration <= 0.0f)
	{
		return;
	}
	// A new hit restarts it.
	RemoveSlow(Villan);
	FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = HeroASC->MakeOutgoingSpec(SlowEffect, 1.0f, Context);
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_MOVE_SPEED_MULTIPLIER, 1.0f - FMath::Clamp(SlowPercent, 0.0f, 90.0f) / 100.0f);
	Spec.Data->DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_SLOWED);

	const TWeakObjectPtr<AZeroLockCharacter> WeakVillan(Villan);
	FQueenSlow& Slow = Slows.Add(WeakVillan);
	Slow.Handle = HeroASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), VillanASC);
	GetWorldTimerManager().SetTimer(Slow.Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakVillan]() { RemoveSlow(WeakVillan); }), SlowDuration, false);
}

void AZL_Solomon_Queen::RemoveSlow(TWeakObjectPtr<AZeroLockCharacter> Villan)
{
	FQueenSlow Slow;
	if (!Slows.RemoveAndCopyValue(Villan, Slow))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(Slow.Timer);
	UBaseCharAbilitySystemComponent* VillanASC = Villan.IsValid() ? Villan->GetMyAbilitySystemComp() : nullptr;
	if (VillanASC && Slow.Handle.IsValid())
	{
		VillanASC->RemoveActiveGameplayEffect(Slow.Handle);
	}
}
