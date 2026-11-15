// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_Ability.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/Tasks/ZL_WaitDelay_Task.h"
#include "Mover/ZeroMoverComponent.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Kit_Ability::UZL_Kit_Ability()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AZeroLockCharacter* UZL_Kit_Ability::GetHero() const
{
	return Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
}

FVector UZL_Kit_Ability::GetAreaOrigin(const AZeroLockCharacter* Hero) const
{
	const FVector Flat = GetHeroAimDirection().GetSafeNormal2D();
	return Hero->GetActorLocation() + Flat * ForwardOffset;
}

void UZL_Kit_Ability::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEnding = false;
	PulsesDone = 0;
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); ASC && CastMontage)
	{
		ASC->PlayMontage(this, ActivationInfo, CastMontage, 1.0f);
	}

	CastDirection = GetHeroAimDirection();
	CastOrigin = GetAreaOrigin(Hero);
	// Owner (predicted) and server; the server's multicast skips the predicting owner.
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), CastCue, Hero->GetActorLocation(), CastDirection.Rotation(), Area.Range * GetAbilityRangeScale());
	FTimerManager& Timers = GetWorld()->GetTimerManager();

	// Self move on the owner: the server gets it through the input stream.
	const bool bMoves = !FMath::IsNearlyZero(MoveForwardSpeed) || !FMath::IsNearlyZero(MoveUpSpeed);
	if (bMoves && IsLocallyControlled())
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			const FVector Forward = bMoveAlongPitch ? CastDirection.GetSafeNormal() : CastDirection.GetSafeNormal2D();
			Mover->RequestSafeAbilityMove(Forward * MoveForwardSpeed + FVector::UpVector * MoveUpSpeed, FMath::Max(MoveDuration, 0.01f));
			if (bStopAfterMove)
			{
				Timers.SetTimer(MoveTimer, this, &UZL_Kit_Ability::OnMoveFinished, FMath::Max(MoveDuration, 0.01f), false);
			}
		}
	}

	if (HasAuthority(&ActivationInfo))
	{
		ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), SelfPayload, GetPower());
		OnKitActivated();

		if (ActiveDuration > 0.0f)
		{
			if (UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp())
			{
				DamageDealtHandle = HeroASC->OnDamageDealt.AddUObject(this, &UZL_Kit_Ability::OnDamageDealt);
			}
		}

		if (bHasAreaHit)
		{
			if (HitDelay <= 0.0f)
			{
				StartPulses();
			}
			else
			{
				Timers.SetTimer(PulseTimer, this, &UZL_Kit_Ability::StartPulses, HitDelay, false);
			}
		}
	}

	const float LastPulse = bHasAreaHit ? HitDelay + (PulseCount - 1) * PulseInterval : 0.0f;
	const float Total = FMath::Max3(ActiveDuration, LastPulse, bMoves ? MoveDuration : 0.0f) + 0.05f;

	if (ActiveDuration > 0.0f && bShowProgressBar)
	{
		UZL_WaitDelay_Task* BarTask = UZL_WaitDelay_Task::WaitDealyWithProgressBar(this, Total);
		BarTask->OnProgress.AddDynamic(this, &UZL_Kit_Ability::UpdateProgressionTimer);
		BarTask->OnStarted.AddDynamic(this, &UZL_Kit_Ability::StartProgressionTimer);
		BarTask->OnEnd.AddDynamic(this, &UZL_Kit_Ability::StopProgressionTimer);
		BarTask->OnFinished.AddDynamic(this, &UZL_Kit_Ability::OnDurationFinished);
		BarTask->ReadyForActivation();
	}
	else
	{
		UAbilityTask_WaitDelay* EndTask = UAbilityTask_WaitDelay::WaitDelay(this, Total);
		EndTask->OnFinish.AddDynamic(this, &UZL_Kit_Ability::OnDurationFinished);
		EndTask->ReadyForActivation();
	}

	if (bEndOnRepress)
	{
		UAbilityTask_WaitInputPress* PressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
		PressTask->OnPress.AddDynamic(this, &UZL_Kit_Ability::OnRepressed);
		PressTask->ReadyForActivation();
	}
}

void UZL_Kit_Ability::StartPulses()
{
	PulseTick();
	if (PulseCount > 1 && IsActive())
	{
		GetWorld()->GetTimerManager().SetTimer(PulseTimer, this, &UZL_Kit_Ability::PulseTick, FMath::Max(PulseInterval, 0.05f), true);
	}
}

void UZL_Kit_Ability::PulseTick()
{
	if (!IsActive() || PulsesDone >= PulseCount)
	{
		GetWorld()->GetTimerManager().ClearTimer(PulseTimer);
		return;
	}
	DoPulse(PulsesDone);
	if (++PulsesDone >= PulseCount)
	{
		GetWorld()->GetTimerManager().ClearTimer(PulseTimer);
	}
}

void UZL_Kit_Ability::DoPulse(int32 PulseIndex)
{
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero || Hero->bIsDead)
	{
		return;
	}

	// Pulses after the first follow the caster (auras, storms around them); the first uses the press's origin.
	FVector Origin = PulseIndex == 0 ? CastOrigin : GetAreaOrigin(Hero);
	if (RandomStrikeSpread > 0.0f)
	{
		const FVector2D Offset = FMath::RandPointInCircle(RandomStrikeSpread * GetAbilityRangeScale());
		Origin += FVector(Offset.X, Offset.Y, 0.0f);
	}
	const float RangeScale = GetAbilityRangeScale();
	const float Power = GetPower();
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), PulseCue, Origin, CastDirection.Rotation(), Area.Range * RangeScale);

	TArray<AZeroLockCharacter*> Villans;
	if (!EnemyPayload.IsEmpty())
	{
		ZLKit::FindHeroes(Hero, Origin, CastDirection, Area, RangeScale, EZL_KitTeam::Enemies, Villans);
		for (AZeroLockCharacter* Villan : Villans)
		{
			if (OnPulseHitEnemy(Villan, PulseIndex))
			{
				ZLKit::ApplyPayload(this, Hero, Villan, Origin, EnemyPayload, Power);
			}
		}
	}

	TArray<AZeroLockCharacter*> Allies;
	if (!AllyPayload.IsEmpty())
	{
		ZLKit::FindHeroes(Hero, Origin, CastDirection, Area, RangeScale, bAllyPayloadOnSelf ? EZL_KitTeam::Allies : EZL_KitTeam::AlliesNotSelf, Allies);
		for (AZeroLockCharacter* Ally : Allies)
		{
			ZLKit::ApplyPayload(this, Hero, Ally, Origin, AllyPayload, Power);
		}
	}

	if (bDrawDebug)
	{
		ZLKit::DrawArea(GetWorld(), Origin, CastDirection, Area, RangeScale, Villans.IsEmpty() ? FColor::Yellow : FColor::Red);
	}
}

void UZL_Kit_Ability::OnDamageDealt(AZeroLockCharacter* Target, float Damage, EZL_ProcTrigger Trigger, const UObject* DamageSource)
{
	// Our own payload damage reports here too: never react to it.
	if (DamageSource == this || !IsActive() || !IsValid(Target))
	{
		return;
	}
	const bool bCounts = WhileActiveTriggers.IsEmpty()
		? (Trigger == EZL_ProcTrigger::BulletHit || Trigger == EZL_ProcTrigger::MeleeHit || Trigger == EZL_ProcTrigger::HeavyMeleeHit)
		: WhileActiveTriggers.Contains(Trigger);
	if (!bCounts)
	{
		return;
	}
	AZeroLockCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}
	if (WhileActiveHitChance <= 0.0f || FMath::FRand() * 100.0f < WhileActiveHitChance)
	{
		ZLKit::ApplyPayload(this, Hero, Target, Hero->GetActorLocation(), WhileActiveHitPayload, GetPower());
	}
	OnWhileActiveHit(Target, Damage, Trigger);
}

void UZL_Kit_Ability::OnMoveFinished()
{
	if (AZeroLockCharacter* Hero = GetHero())
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			Mover->RequestSafeStop();
		}
	}
}

void UZL_Kit_Ability::OnDurationFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_Kit_Ability::OnRepressed(float TimeWaited)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_Kit_Ability::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding)
	{
		return;
	}
	bEnding = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PulseTimer);
		World->GetTimerManager().ClearTimer(MoveTimer);
	}
	if (DamageDealtHandle.IsValid())
	{
		if (AZeroLockCharacter* Hero = GetHero(); Hero && Hero->GetMyAbilitySystemComp())
		{
			Hero->GetMyAbilitySystemComp()->OnDamageDealt.Remove(DamageDealtHandle);
		}
		DamageDealtHandle.Reset();
	}

	// The client's timers run first, and replicating that end could cancel the server before its pulses.
	// Real cancels still reach the server: CancelAbility replicates on its own.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
