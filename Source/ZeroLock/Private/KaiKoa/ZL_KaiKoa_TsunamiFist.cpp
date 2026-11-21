// Copyright Preetham Mukundan (C) 2026


#include "KaiKoa/ZL_KaiKoa_TsunamiFist.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_KaiKoa_TsunamiFist::UZL_KaiKoa_TsunamiFist()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	AbilityInputID = EGASAbilityInputID::Ability_1;
	Slot = EGameplayAbilitySlot::AbilitySlot2;
	bIsMovementAbility = true;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_KAIKOA_TSUNAMIFIST);
	CooldownDuration = 10.0f;
	AbilityName = TEXT("Tsunami Fist");
	AbilityDescription = TEXT("Lunge forward and punch a wave in front of you: 70 spirit damage and a knockback to every enemy in it.");
}

void UZL_KaiKoa_TsunamiFist::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PunchDirection = GetHeroAimDirection().GetSafeNormal2D();
	if (PunchDirection.IsNearlyZero())
	{
		PunchDirection = Hero->GetActorForwardVector().GetSafeNormal2D();
	}
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), CastCue, Hero->GetActorLocation(), PunchDirection.Rotation());

	// The owner predicts the lunge; the server gets it through the input stream.
	if (IsLocallyControlled() && LungeSpeed > 0.0f)
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			Mover->RequestSafeAbilityMove(PunchDirection * LungeSpeed, FMath::Max(LungeDuration, 0.01f));
		}
	}

	if (HasAuthority(&ActivationInfo))
	{
		UAbilityTask_WaitDelay* HitTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(HitDelay, 0.0f));
		HitTask->OnFinish.AddDynamic(this, &UZL_KaiKoa_TsunamiFist::OnHitDelayFinished);
		HitTask->ReadyForActivation();
	}

	UAbilityTask_WaitDelay* EndTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(HitDelay, LungeDuration) + 0.05f);
	EndTask->OnFinish.AddDynamic(this, &UZL_KaiKoa_TsunamiFist::OnFinished);
	EndTask->ReadyForActivation();
}

void UZL_KaiKoa_TsunamiFist::OnHitDelayFinished()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && !Hero->bIsDead)
	{
		Punch(Hero);
	}
}

void UZL_KaiKoa_TsunamiFist::Punch(AZeroLockCharacter* Hero)
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	if (!HeroASC)
	{
		return;
	}

	const int32 Level = GetAbilityLevel();
	const float Range = HitRange.GetValueAtLevel(Level) * GetAbilityRangeScale();
	// From where the lunge brought him.
	const FVector Origin = Hero->GetActorLocation();
	ZLKitCue::Play(HeroASC, WaveCue, Origin, PunchDirection.Rotation(), Range);

	TArray<AZeroLockCharacter*> Villans;
	FindTargets(Hero, Origin, PunchDirection, Range, Villans);

	const float Damage = SpiritDamage.GetValueAtLevel(Level);
	for (AZeroLockCharacter* Villan : Villans)
	{
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		HeroASC->ApplySpiritDamage(VillanASC, Damage, this);
		if (Villan->bIsDead)
		{
			continue;
		}

		FVector Away = (Villan->GetActorLocation() - Origin).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = PunchDirection;
		}
		const FVector Velocity = Away * KnockbackSpeed + FVector::UpVector * KnockUpSpeed;
		SendVictimMoveEvent(Hero, Villan, TEXT("Zerolock.Movement.Forced"), FMath::Max(KnockbackDuration, 0.05f), &Velocity);
		ZLKitCue::Play(VillanASC, HitHeroCue, Villan->GetActorLocation(), Away.Rotation());
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugCone(GetWorld(), Origin, PunchDirection, Range, FMath::DegreesToRadians(HitHalfAngle), FMath::DegreesToRadians(15.0f), 12,
			Villans.Num() > 0 ? FColor::Green : FColor::Red, false, 2.0f);
	}
#endif
}

void UZL_KaiKoa_TsunamiFist::FindTargets(AZeroLockCharacter* Hero, const FVector& Origin, const FVector& Forward, float Range,
	TArray<AZeroLockCharacter*>& OutVillans) const
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_KaiKoa_TsunamiFist), false, Hero);

	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(Range + 50.0f), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || OutVillans.Contains(Villan))
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (!VillanASC || HeroASC->IsUntouchable(VillanASC))
		{
			continue;
		}

		// Flat cone; heroes hugging Kai count whatever the angle.
		const FVector ToVillan = Villan->GetActorLocation() - Origin;
		const FVector ToVillanFlat(ToVillan.X, ToVillan.Y, 0.0f);
		const float CapsuleRadius = Villan->GetCapsuleComponent() ? Villan->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.0f;
		const float Distance = ToVillanFlat.Size();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToVillanFlat.GetSafeNormal(), Forward), -1.0f, 1.0f)));
		if (Distance > Range + CapsuleRadius || (Distance >= CapsuleRadius * 2.0f && Angle > HitHalfAngle))
		{
			continue;
		}

		// Walls block the wave.
		FHitResult Hit;
		FCollisionQueryParams LOSParams(SCENE_QUERY_STAT(ZL_KaiKoa_TsunamiFistLOS), false, Hero);
		LOSParams.AddIgnoredActor(Villan);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Origin, Villan->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), LOSParams))
		{
			continue;
		}
		OutVillans.Add(Villan);
	}
}

void UZL_KaiKoa_TsunamiFist::OnFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_KaiKoa_TsunamiFist::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The client reaches its end first; replicating it would end the server's instance before the punch. The server ends
	// its own and replicates that. Real cancels still go through CancelAbility.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
