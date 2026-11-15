// Copyright Preetham Mukundan (C) 2026

#include "HeroKit/ZL_Kit_TargetAbility.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/BaseGameplayAbility.h"
#include "TimerManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UZL_Kit_TargetAbility::ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && Target && Hero->HasAuthority())
	{
		ZLKit::ApplyPayload(this, Hero, Hero, Hero->GetActorLocation(), SelfPayload, PowerScale.GetValueAtLevel(GetAbilityLevel()));
		ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), CastCue, Hero->GetActorLocation(), (Target->GetActorLocation() - Hero->GetActorLocation()).Rotation());
		OnTargetConfirmedServer(Hero, Target);
		DoMove(Hero, Target);

		for (int32 Pulse = 0; Pulse < PulseCount; ++Pulse)
		{
			const float Delay = HitDelay + Pulse * PulseInterval;
			if (Delay <= 0.0f)
			{
				DoPulse(Target, Pulse);
				continue;
			}
			TWeakObjectPtr<UZL_Kit_TargetAbility> WeakThis(this);
			TWeakObjectPtr<AZeroLockCharacter> WeakTarget(Target);
			FTimerHandle Handle;
			Hero->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Hero, [WeakThis, WeakTarget, Pulse]()
			{
				if (WeakThis.IsValid() && WeakTarget.IsValid())
				{
					WeakThis->DoPulse(WeakTarget.Get(), Pulse);
				}
			}), Delay, false);
		}
	}
	Super::ConeTargetConfirmed(Target, Data);
}

void UZL_Kit_TargetAbility::DoPulse(AZeroLockCharacter* Target, int32 PulseIndex)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || Hero->bIsDead || !Target || Target->bIsDead)
	{
		return;
	}
	ZLKit::ApplyPayload(this, Hero, Target, Hero->GetActorLocation(), GetPulsePayload(Hero, Target, PulseIndex), PowerScale.GetValueAtLevel(GetAbilityLevel()));
}

void UZL_Kit_TargetAbility::DoMove(AZeroLockCharacter* Hero, AZeroLockCharacter* Target)
{
	if (Target == Hero)
	{
		return;
	}
	const FVector HeroLocation = Hero->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector HeroToTarget = (TargetLocation - HeroLocation).GetSafeNormal2D();

	switch (Move)
	{
	case EZL_KitTargetMove::DashToTarget:
	{
		const FVector End = TargetLocation - HeroToTarget * 80.0f;
		UBaseGameplayAbility::SendVictimMoveEvent(Hero, Hero, TEXT("Zerolock.Movement.PullTo"), MoveTime, &End);
		break;
	}
	case EZL_KitTargetMove::TeleportBehind:
	{
		const FVector Behind = TargetLocation - Target->GetActorForwardVector().GetSafeNormal2D() * MoveOffset;
		Hero->ServerForceTeleport(Behind, (TargetLocation - Behind).Rotation());
		break;
	}
	case EZL_KitTargetMove::SwapPlaces:
		Hero->ServerForceTeleport(TargetLocation, (HeroLocation - TargetLocation).Rotation());
		Target->ServerForceTeleport(HeroLocation, (TargetLocation - HeroLocation).Rotation());
		break;
	case EZL_KitTargetMove::PullTargetToCaster:
	{
		const FVector End = HeroLocation + HeroToTarget * MoveOffset;
		UBaseGameplayAbility::SendVictimMoveEvent(Hero, Target, TEXT("Zerolock.Movement.PullTo"), MoveTime, &End);
		break;
	}
	default:
		break;
	}
}
