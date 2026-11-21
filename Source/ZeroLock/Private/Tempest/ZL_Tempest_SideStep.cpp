// Copyright Preetham Mukundan (C) 2026


#include "Tempest/ZL_Tempest_SideStep.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_StatusTimerAbility.h"
#include "Items/ZL_GE_ItemStats.h"
#include "Mover/ZeroMoverComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Tempest_SideStep::UZL_Tempest_SideStep()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	AbilityInputID = EGASAbilityInputID::Ability_1;
	Slot = EGameplayAbilitySlot::AbilitySlot2;
	bIsMovementAbility = true;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_TEMPEST_SIDESTEP);
	CooldownDuration = 16.0f;
	AbilityName = TEXT("Side Step");
	AbilityDescription = TEXT("Leap the way you're moving (forward if standing still). Hits enemies around you for 40 on take-off, and for 70 plus a 0.8 s stun where you land.");
}

void UZL_Tempest_SideStep::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UZeroMoverComponent* Mover = Hero ? Hero->GetZeroMoverComponent() : nullptr;
	if (!Hero || !Mover || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	LaunchDirection = Hero->GetInputWorldDir().GetSafeNormal2D();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = GetHeroAimDirection().GetSafeNormal2D();
	}
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = Hero->GetActorForwardVector().GetSafeNormal2D();
	}

	const float Radius = HitRadius.GetValueAtLevel(GetAbilityLevel()) * GetAbilityRangeScale();
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), LaunchCue, Hero->GetActorLocation(), LaunchDirection.Rotation(), Radius);

	// The owner predicts the leap; the server gets it through the input stream.
	if (IsLocallyControlled())
	{
		Mover->RequestSafeAbilityMove(LaunchDirection * LaunchFlatSpeed + FVector::UpVector * LaunchUpSpeed, FMath::Max(LaunchDuration, 0.01f));
	}
	if (HasAuthority(&ActivationInfo))
	{
		HitAround(Hero, TakeoffDamage.GetValueAtLevel(GetAbilityLevel()), 0.0f);
	}

	// Every machine watches its own Mover for the landing. A cast in the air already counts as off the ground; on the
	// ground the leap's switch to an air mode arms it (the server simulates that a bit after activation).
	bLeftGround = !IsGroundMode(Mover->GetMovementModeName());
	Mover->OnMovementModeChanged.AddUniqueDynamic(this, &UZL_Tempest_SideStep::OnMovementModeChanged);
	bLandingBound = true;

	UAbilityTask_WaitDelay* TimeoutTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(LandingTimeout, 0.5f));
	TimeoutTask->OnFinish.AddDynamic(this, &UZL_Tempest_SideStep::OnLandingTimeout);
	TimeoutTask->ReadyForActivation();
}

bool UZL_Tempest_SideStep::IsGroundMode(const FName& ModeName)
{
	// Ziplining isn't a landing: grabbing a cable mid-leap keeps waiting for the real touchdown.
	return ModeName == DefaultModeNames::Walking || ModeName == FName(TEXT("Sliding"));
}

void UZL_Tempest_SideStep::OnMovementModeChanged(const FName& PreviousMovementModeName, const FName& NewMovementModeName)
{
	if (!IsActive())
	{
		UnbindLanding();
		return;
	}
	// Resimulated steps count too: only the server hits, and skipping them could miss a touchdown that only shows up
	// after a correction.
	if (!IsGroundMode(NewMovementModeName))
	{
		bLeftGround = true;
		return;
	}
	// Walking -> Sliding before the leap has happened on this machine isn't a landing.
	if (bLeftGround && !IsGroundMode(PreviousMovementModeName))
	{
		Land();
	}
}

void UZL_Tempest_SideStep::Land()
{
	UnbindLanding();
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && !Hero->bIsDead)
	{
		const float Radius = HitRadius.GetValueAtLevel(GetAbilityLevel()) * GetAbilityRangeScale();
		const FVector Location = Hero->GetActorLocation();
		// The owner plays its copy the moment it lands here; the server sends the cue to everyone else under the
		// activation's prediction key, so its multicast skips the owner (no double, no wait).
		if (!HasAuthority(&CurrentActivationInfo) && IsLocallyControlled() && LandCue.Duration <= 0.0f)
		{
			ZLKitCue::ExecuteLocal(Hero, LandCue.Cue, Location, LaunchDirection.Rotation(), Radius);
		}
		if (HasAuthority(&CurrentActivationInfo))
		{
			{
				FScopedPredictionWindow Window(Hero->GetMyAbilitySystemComp(), CurrentActivationInfo.GetActivationPredictionKey(), false);
				ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), LandCue, Location, LaunchDirection.Rotation(), Radius);
			}
			const int32 Level = GetAbilityLevel();
			HitAround(Hero, LandingDamage.GetValueAtLevel(Level), LandingStun.GetValueAtLevel(Level));
		}
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Tempest_SideStep::HitAround(AZeroLockCharacter* Hero, float Damage, float StunDuration) const
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	if (!HeroASC)
	{
		return;
	}
	const float Radius = HitRadius.GetValueAtLevel(GetAbilityLevel()) * GetAbilityRangeScale();
	const FVector Origin = Hero->GetActorLocation();

	// Heroes use the Hero object type (ECC_GameTraceChannel1).
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_Tempest_SideStep), false, Hero);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
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
		// Walls block it.
		FHitResult WallHit;
		FCollisionQueryParams LOSParams(SCENE_QUERY_STAT(ZL_Tempest_SideStepLOS), false, Hero);
		LOSParams.AddIgnoredActor(Villan);
		if (GetWorld()->LineTraceSingleByObjectType(WallHit, Origin, Villan->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), LOSParams))
		{
			continue;
		}
		Hit.Add(Villan);

		HeroASC->ApplySpiritDamage(VillanASC, Damage, this);
		if (StunDuration > 0.0f && !Villan->bIsDead)
		{
			ZLItemStatus::Apply(HeroASC, VillanASC, EZL_ItemStatus::Stun, StunDuration);
			UZL_StatusTimerAbility::SendStatusTimer(Hero, VillanASC, ZLItemStatus::GetTag(EZL_ItemStatus::Stun), StunDuration);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, Radius, 16, Hit.Num() > 0 ? FColor::Red : FColor::Yellow, false, 1.5f);
	}
#endif
}

void UZL_Tempest_SideStep::OnLandingTimeout()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Tempest_SideStep::UnbindLanding()
{
	if (!bLandingBound)
	{
		return;
	}
	bLandingBound = false;
	if (AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UZeroMoverComponent* Mover = Hero->GetZeroMoverComponent())
		{
			Mover->OnMovementModeChanged.RemoveDynamic(this, &UZL_Tempest_SideStep::OnMovementModeChanged);
		}
	}
}

void UZL_Tempest_SideStep::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UnbindLanding();
	// The client lands before the server; replicating its end would cancel the server's instance before the landing hit.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
