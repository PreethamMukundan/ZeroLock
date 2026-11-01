// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_SetupBishop.h"

#include "Solomon/ZL_Solomon_Bishop.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Solomon_SetupBishop::UZL_Solomon_SetupBishop()
{
	SurfaceMode = EZL_SurfaceTargetMode::Ground;
	TargetConfirmationStyle = EGASTargetConfirmationStyle::Instant;
	bCancelOnRepress = false;
	// Deadlock AbilityCastRange 20 m (1 m = 70 cm), AbilityCooldown 12.
	MaxPlacementRange = 1400.0f;
	CooldownDuration = 12.0f;
	BishopClass = AZL_Solomon_Bishop::StaticClass();
}

int32 UZL_Solomon_SetupBishop::CountBishops(const AActor* Avatar) const
{
	return AZL_Solomon_ChessPiece::GetPiecesOf(Avatar, BishopClass ? *BishopClass : AZL_Solomon_Bishop::StaticClass()).Num();
}

bool UZL_Solomon_SetupBishop::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// The cooldown only starts when they break, so with bishops out it's never running anyway.
	if (CountBishops(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr) > 0)
	{
		return true;
	}
	return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UZL_Solomon_SetupBishop::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// Placing is free: the base commits on every placement, only the break's commit counts.
	if (bBreaking)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
}

void UZL_Solomon_SetupBishop::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (CountBishops(Avatar) < MaxBishops)
	{
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		return;
	}

	// Break: the server removes the bishops (they replicate away); both sides run the commit for the cooldown.
	bBreaking = true;
	const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo);
	bBreaking = false;
	if (bCommitted && HasAuthority(&ActivationInfo))
	{
		for (AZL_Solomon_ChessPiece* Bishop : AZL_Solomon_ChessPiece::GetPiecesOf(Avatar, BishopClass ? *BishopClass : AZL_Solomon_Bishop::StaticClass()))
		{
			Bishop->Destroy();
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bCommitted);
}

bool UZL_Solomon_SetupBishop::GetPlacementTransform(const FHitResult& Hit, FTransform& OutTransform) const
{
	const AZL_Solomon_Bishop* Defaults = BishopClass ? BishopClass->GetDefaultObject<AZL_Solomon_Bishop>() : nullptr;
	const AActor* Hero = GetAvatarActorFromActorInfo();
	if (!Defaults || !Hero)
	{
		return Super::GetPlacementTransform(Hit, OutTransform);
	}
	FVector Floor;
	const bool bValid = Defaults->FindPlacementSpot(Hero, Hit.ImpactPoint, true, Floor);
	// X = up, like the base's ground placement; Z along world X so a preview box lines up with the world axes.
	OutTransform = FTransform(FRotationMatrix::MakeFromXZ(FVector::UpVector, FVector::ForwardVector).ToQuat(), Floor);
	return bValid;
}

void UZL_Solomon_SetupBishop::PlacementConfirmed(const FHitResult& Hit, const FTransform& Placement, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && BishopClass && HasAuthority(&CurrentActivationInfo) && CountBishops(Hero) < MaxBishops)
	{
		// Face away from Solomon; pivot on the floor in the middle of the square.
		FVector Facing = (Placement.GetLocation() - Hero->GetActorLocation()).GetSafeNormal2D();
		if (Facing.IsNearlyZero())
		{
			Facing = Hero->GetActorForwardVector();
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Hero;
		SpawnParams.Instigator = Hero;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AZL_Solomon_Bishop>(BishopClass, Placement.GetLocation(), Facing.Rotation(), SpawnParams);
	}
	Super::PlacementConfirmed(Hit, Placement, Data);
}
