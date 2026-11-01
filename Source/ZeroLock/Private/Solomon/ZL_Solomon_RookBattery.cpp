// Copyright Preetham Mukundan (C) 2026


#include "Solomon/ZL_Solomon_RookBattery.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Engine/World.h"
#include "Solomon/ZL_Solomon_Rook.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Solomon_RookBattery::UZL_Solomon_RookBattery()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// Deadlock AbilityCooldown 33 (-12 at T3).
	CooldownDuration = 33.0f;
	RookClass = AZL_Solomon_Rook::StaticClass();
}

void UZL_Solomon_RookBattery::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEnded = false;
	Rooks.Reset();

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !RookClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (CastMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, CastMontage, 1.0f, NAME_None, false);
		MontageTask->ReadyForActivation();
	}

	CastDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(CastDelay, 0.01f));
	CastDelayTask->OnFinish.AddDynamic(this, &UZL_Solomon_RookBattery::OnCastDelayFinished);
	CastDelayTask->ReadyForActivation();
}

void UZL_Solomon_RookBattery::OnCastDelayFinished()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		SpawnRooks();
		if (Rooks.IsEmpty())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
	}

	// Re-press = fork. The server sees the owning client's press as a replicated event (cached if it came early).
	RecastTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	RecastTask->OnPress.AddDynamic(this, &UZL_Solomon_RookBattery::OnRecastPressed);
	RecastTask->ReadyForActivation();
}

void UZL_Solomon_RookBattery::SpawnRooks()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero)
	{
		return;
	}
	const int32 Level = GetAbilityLevel();

	FVector Heading = GetHeroAimDirection();
	Heading.Z = 0.0f;
	Heading = Heading.GetSafeNormal();
	if (Heading.IsNearlyZero())
	{
		Heading = FVector(Hero->GetActorForwardVector().X, Hero->GetActorForwardVector().Y, 0.0f).GetSafeNormal();
	}
	const FVector Right(-Heading.Y, Heading.X, 0.0f);

	float CapsuleRadius = 0.0f;
	float CapsuleHalfHeight = 0.0f;
	Hero->GetSimpleCollisionCylinder(CapsuleRadius, CapsuleHalfHeight);
	const FVector Center = Hero->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RookBatterySpawn), false, Hero);
	AZL_Solomon_Rook* Spawned[2] = {nullptr, nullptr};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		// Index 0 = left, 1 = right.
		const float Side = Index == 0 ? -1.0f : 1.0f;
		// Low by his feet (the rook settles onto the floor on its first step), never inside a wall beside him.
		const FVector Low = Center - FVector(0.0f, 0.0f, CapsuleHalfHeight - 30.0f);
		FVector Location = Low + Right * Side * DeployGap * 0.5f;
		FHitResult Hit;
		if (GetWorld()->SweepSingleByObjectType(Hit, Low, Location, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
			FCollisionShape::MakeSphere(20.0f), Params))
		{
			Location = Hit.Location;
		}

		const FTransform Transform(Heading.Rotation(), Location);
		AZL_Solomon_Rook* Rook = GetWorld()->SpawnActorDeferred<AZL_Solomon_Rook>(RookClass, Transform, Hero, Hero,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Rook)
		{
			continue;
		}
		Rook->InitRook(Damage.GetValueAtLevel(Level), ImmobilizeDuration.GetValueAtLevel(Level), RookSpeed.GetValueAtLevel(Level),
			MaxTravelDistance.GetValueAtLevel(Level), ForkDistance.GetValueAtLevel(Level),
			FMath::FloorToInt(MaxReflects.GetValueAtLevel(Level)), Right * Side);
		Rook->FinishSpawning(Transform);
		if (IsValid(Rook))
		{
			Spawned[Index] = Rook;
			Rooks.Add(Rook);
			Rook->OnDestroyed.AddDynamic(this, &UZL_Solomon_RookBattery::OnRookDestroyed);
		}
	}
	if (Spawned[0] && Spawned[1])
	{
		Spawned[0]->SetPartner(Spawned[1]);
		Spawned[1]->SetPartner(Spawned[0]);
	}
}

void UZL_Solomon_RookBattery::OnRecastPressed(float TimeWaited)
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		for (const TWeakObjectPtr<AZL_Solomon_Rook>& Rook : Rooks)
		{
			if (Rook.IsValid())
			{
				Rook->Fork();
			}
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	// The server ends its side when it gets the press.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UZL_Solomon_RookBattery::OnRookDestroyed(AActor* DestroyedActor)
{
	for (const TWeakObjectPtr<AZL_Solomon_Rook>& Rook : Rooks)
	{
		if (Rook.IsValid() && Rook.Get() != DestroyedActor && !Rook->IsActorBeingDestroyed())
		{
			return;
		}
	}
	// Both gone: nothing left to fork.
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UZL_Solomon_RookBattery::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bEnded)
	{
		bEnded = true;
		if (CastDelayTask)
		{
			CastDelayTask->OnFinish.RemoveDynamic(this, &UZL_Solomon_RookBattery::OnCastDelayFinished);
			CastDelayTask->EndTask();
			CastDelayTask = nullptr;
		}
		if (RecastTask)
		{
			RecastTask->OnPress.RemoveDynamic(this, &UZL_Solomon_RookBattery::OnRecastPressed);
			RecastTask->EndTask();
			RecastTask = nullptr;
		}
		// The rooks carry on without the ability.
		for (const TWeakObjectPtr<AZL_Solomon_Rook>& Rook : Rooks)
		{
			if (Rook.IsValid())
			{
				Rook->OnDestroyed.RemoveDynamic(this, &UZL_Solomon_RookBattery::OnRookDestroyed);
			}
		}
		Rooks.Reset();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
