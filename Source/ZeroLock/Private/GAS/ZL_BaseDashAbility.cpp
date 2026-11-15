// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_BaseDashAbility.h"

#include "Components/CapsuleComponent.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Mover/ZeroMoverComponent.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_BaseDashAbility::UZL_BaseDashAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bIsMovementAbility = true;
	bCancelOnStun = true;
}

void UZL_BaseDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 Level = GetAbilityLevel();
	const float Duration = FMath::Max(DashDuration.GetValueAtLevel(Level), 0.01f);
	const float Distance = DashDistance.GetValueAtLevel(Level) * GetAbilityRangeScale();
	const FVector Direction = GetDashDirection().GetSafeNormal();

	bDashActive = true;
	DashHits.Reset();

	// The server ignores latches for remote-controlled pawns: the owner's request reaches it through the input.
	if (IsLocallyControlled())
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			Mover->RequestSafeAbilityMove(Direction * (Distance / Duration), Duration);
		}
	}

	OnDashStart(Direction);
	if (!bDashActive)
	{
		return;
	}

	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (HasAuthority(&ActivationInfo))
	{
		LastSweepLocation = Hero->GetActorLocation();
		Timers.SetTimer(HitCheckTimer, this, &UZL_BaseDashAbility::CheckDashHits, HitCheckInterval, true);
	}
	Timers.SetTimer(FinishTimer, this, &UZL_BaseDashAbility::FinishDash, Duration + (IsLocallyControlled() ? 0.0f : ServerHitGraceTime), false);
}

void UZL_BaseDashAbility::CheckDashHits()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	const UCapsuleComponent* Capsule = Hero ? Hero->GetCapsuleComponent() : nullptr;
	if (!HeroASC || !Capsule)
	{
		return;
	}

	const float Padding = HitPadding.GetValueAtLevel(GetAbilityLevel());
	const float Radius = Capsule->GetScaledCapsuleRadius() + Padding;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() + Padding;
	const FVector SweepStart = LastSweepLocation;
	const FVector SweepEnd = Hero->GetActorLocation();
	LastSweepLocation = SweepEnd;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_BaseDash), false, Hero);
	TArray<FHitResult> Hits;
	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	GetWorld()->SweepMultiByObjectType(Hits, SweepStart, SweepEnd, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDashDebug)
	{
		ZLDebugDraw::Capsule(GetWorld(), SweepEnd, HalfHeight, Radius, Hits.IsEmpty() ? FColor::Green : FColor::Red, 2.0f);
	}
#endif

	for (const FHitResult& Hit : Hits)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Hit.GetActor());
		if (!IsValid(Villan) || Villan->bIsDead || DashHits.Contains(Villan) || Hero->IsOnSameTeam(Villan))
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (!VillanASC || HeroASC->IsUntouchable(VillanASC))
		{
			continue;
		}

		DashHits.Add(Villan);

#if ENABLE_DRAW_DEBUG
		if (bDrawDashDebug)
		{
			ZLDebugDraw::Sphere(GetWorld(), Villan->GetActorLocation(), 60.0f, FColor::Red, 2.0f);
		}
#endif

		OnDashHit(Villan, Hit);
		if (!bDashActive)
		{
			return;
		}
	}
}

void UZL_BaseDashAbility::FinishDash()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		CheckDashHits();
	}
	if (bDashActive)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UZL_BaseDashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The client finishes first, and replicating that would cancel the server's instance before its last hit sweeps. The
	// server ends its own instance instead. Real cancels still reach it: CancelAbility replicates on its own.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}

	if (bDashActive)
	{
		bDashActive = false;

		FTimerManager& Timers = GetWorld()->GetTimerManager();
		Timers.ClearTimer(HitCheckTimer);
		Timers.ClearTimer(FinishTimer);

		const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
		UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
		if (Mover && ActorInfo && ActorInfo->IsLocallyControlled() && (bStopAtEnd || bWasCancelled))
		{
			Mover->RequestSafeStop();
		}

		OnDashEnd(bWasCancelled);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UZL_BaseDashAbility::GetDashDirection_Implementation() const
{
	return GetHeroAimDirection();
}

void UZL_BaseDashAbility::OnDashStart_Implementation(const FVector& Direction)
{
}

void UZL_BaseDashAbility::OnDashHit_Implementation(AZeroLockCharacter* Villan, const FHitResult& Hit)
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (!HeroASC || !IsValid(Villan))
	{
		return;
	}

	const float Damage = DashDamage.GetValueAtLevel(GetAbilityLevel());
	switch (DashDamageType)
	{
	case EZL_DashDamageType::Weapon:
		HeroASC->ApplyWeaponDamage(Villan->GetMyAbilitySystemComp(), Damage, this);
		break;
	case EZL_DashDamageType::Melee:
		HeroASC->ApplyMeleeDamage(Villan->GetMyAbilitySystemComp(), Damage, this);
		break;
	default:
		HeroASC->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), Damage, this);
		break;
	}
}

void UZL_BaseDashAbility::OnDashEnd_Implementation(bool bWasCancelled)
{
}
