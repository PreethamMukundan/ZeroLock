// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_DevelopQueen.h"

#include "GameplayEffect.h"
#include "Solomon/ZL_Solomon_Queen.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Solomon_DevelopQueen::UZL_Solomon_DevelopQueen()
{
	SurfaceMode = EZL_SurfaceTargetMode::Ground;
	TargetConfirmationStyle = EGASTargetConfirmationStyle::Confirm;
	// Deadlock AbilityCastRange 30 m (1 m = 70 cm), AbilityCooldown 210 (-40 at T1).
	MaxPlacementRange = 2100.0f;
	CooldownDuration = 210.0f;
	QueenClass = AZL_Solomon_Queen::StaticClass();

	// Preview: the crash area (the queen's CrashRadius 213), flat on the floor (placement X = up).
	bShowPreview = true;
	PreviewSize = FVector(5.0f, 426.0f, 426.0f);
}

AZL_Solomon_Queen* UZL_Solomon_DevelopQueen::FindQueen(const AActor* Avatar) const
{
	const TArray<AZL_Solomon_ChessPiece*> Queens = AZL_Solomon_ChessPiece::GetPiecesOf(Avatar, AZL_Solomon_Queen::StaticClass());
	return Queens.Num() > 0 ? Cast<AZL_Solomon_Queen>(Queens[0]) : nullptr;
}

bool UZL_Solomon_DevelopQueen::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// The recall works while the cast's cooldown runs.
	if (FindQueen(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		return true;
	}
	return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UZL_Solomon_DevelopQueen::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!FindQueen(Avatar))
	{
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		return;
	}

	// Recall: free, the server removes the queen (it replicates away).
	if (HasAuthority(&ActivationInfo))
	{
		for (AZL_Solomon_ChessPiece* Queen : AZL_Solomon_ChessPiece::GetPiecesOf(Avatar, AZL_Solomon_Queen::StaticClass()))
		{
			Queen->Destroy();
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UZL_Solomon_DevelopQueen::GetPlacementTransform(const FHitResult& Hit, FTransform& OutTransform) const
{
	const AZL_Solomon_Queen* Defaults = QueenClass ? QueenClass->GetDefaultObject<AZL_Solomon_Queen>() : nullptr;
	const AActor* Hero = GetAvatarActorFromActorInfo();
	if (!Defaults || !Hero)
	{
		return Super::GetPlacementTransform(Hit, OutTransform);
	}
	// The board square under the aim (or a neighbour), no spacing check: the old queen is replaced.
	FVector Floor;
	const bool bValid = Defaults->FindPlacementSpot(Hero, Hit.ImpactPoint, false, Floor);
	// X = up, like the base's ground placement; Z along world X so the preview lines up with the world axes (and the squares).
	OutTransform = FTransform(FRotationMatrix::MakeFromXZ(FVector::UpVector, FVector::ForwardVector).ToQuat(), Floor);
	return bValid;
}

void UZL_Solomon_DevelopQueen::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && QueenClass && HasAuthority(&CurrentActivationInfo))
	{
		// One queen (Deadlock MaxPieces 1).
		for (AZL_Solomon_ChessPiece* Old : AZL_Solomon_ChessPiece::GetPiecesOf(Hero, AZL_Solomon_Queen::StaticClass()))
		{
			Old->Destroy();
		}

		const int32 Level = GetAbilityLevel();
		const FVector Land = Placement.GetLocation();
		FVector Facing = (Land - Hero->GetActorLocation()).GetSafeNormal2D();
		if (Facing.IsNearlyZero())
		{
			Facing = Hero->GetActorForwardVector();
		}
		const float DropHeight = QueenClass->GetDefaultObject<AZL_Solomon_Queen>()->GetDropHeight();
		const FTransform Transform(Facing.Rotation(), Land + FVector(0.0f, 0.0f, DropHeight));
		if (AZL_Solomon_Queen* Queen = GetWorld()->SpawnActorDeferred<AZL_Solomon_Queen>(QueenClass, Transform, Hero, Hero,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Queen->InitQueen(Land, Damage.GetValueAtLevel(Level), StunDuration.GetValueAtLevel(Level), SlowPercent.GetValueAtLevel(Level),
				SlowDuration.GetValueAtLevel(Level), FMath::FloorToInt(TileSteps.GetValueAtLevel(Level)), FMath::FloorToInt(Moves.GetValueAtLevel(Level)),
				StunEffect);
			Queen->FinishSpawning(Transform);
		}
	}
	Super::PlacementConfirmed(Hit, Placement, Data);
}
