// Copyright Preetham Mukundan (C) 2026


#include "KaiKoa/ZL_KaiKoa_AbsoluteZero.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_StatusTimerAbility.h"
#include "Items/ZL_GE_ItemStats.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_KaiKoa_AbsoluteZero::UZL_KaiKoa_AbsoluteZero()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	AbilityInputID = EGASAbilityInputID::Ultimate;
	Slot = EGameplayAbilitySlot::UltimateSlot;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_KAIKOA_ABSOLUTEZERO);
	CooldownDuration = 90.0f;
	AbilityName = TEXT("Absolute Zero");
	AbilityDescription = TEXT("Freeze everything around you: every enemy within 9 m takes 90 spirit damage and is frozen (stunned and rooted) for 3 s.");
}

void UZL_KaiKoa_AbsoluteZero::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (CastMontage)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->PlayMontage(this, ActivationInfo, CastMontage, 1.0f);
		}
	}
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), CastCue, Hero->GetActorLocation(), Hero->GetActorRotation(),
		Radius.GetValueAtLevel(GetAbilityLevel()));

	if (HasAuthority(&ActivationInfo))
	{
		UAbilityTask_WaitDelay* FreezeTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(FreezeDelay, 0.0f));
		FreezeTask->OnFinish.AddDynamic(this, &UZL_KaiKoa_AbsoluteZero::OnFreezeDelayFinished);
		FreezeTask->ReadyForActivation();
	}

	UAbilityTask_WaitDelay* EndTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(FreezeDelay, 0.0f) + 0.05f);
	EndTask->OnFinish.AddDynamic(this, &UZL_KaiKoa_AbsoluteZero::OnFinished);
	EndTask->ReadyForActivation();
}

void UZL_KaiKoa_AbsoluteZero::OnFreezeDelayFinished()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero && !Hero->bIsDead)
	{
		Freeze(Hero);
	}
}

void UZL_KaiKoa_AbsoluteZero::Freeze(AZeroLockCharacter* Hero)
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	if (!HeroASC)
	{
		return;
	}

	const int32 Level = GetAbilityLevel();
	const float FreezeRadius = Radius.GetValueAtLevel(Level) * GetAbilityRangeScale();
	const float Damage = SpiritDamage.GetValueAtLevel(Level);
	const float Duration = FreezeDuration.GetValueAtLevel(Level);
	const FVector Origin = Hero->GetActorLocation();
	ZLKitCue::Play(HeroASC, StrikeCue, Origin, Hero->GetActorRotation(), FreezeRadius);

	// Heroes use the Hero object type (ECC_GameTraceChannel1). No line of sight: it freezes the whole area.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_KaiKoa_AbsoluteZero), false, Hero);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1),
		FCollisionShape::MakeSphere(FreezeRadius), QueryParams);

	TSet<AZeroLockCharacter*> Frozen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (!IsValid(Villan) || Villan == Hero || Villan->bIsDead || Hero->IsOnSameTeam(Villan) || Frozen.Contains(Villan))
		{
			continue;
		}
		UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
		if (!VillanASC || HeroASC->IsUntouchable(VillanASC))
		{
			continue;
		}
		Frozen.Add(Villan);

		HeroASC->ApplySpiritDamage(VillanASC, Damage, this);
		if (Villan->bIsDead || Duration <= 0.0f)
		{
			continue;
		}

		for (const EZL_ItemStatus Status : { EZL_ItemStatus::Stun, EZL_ItemStatus::Root })
		{
			ZLItemStatus::Apply(HeroASC, VillanASC, Status, Duration);
			UZL_StatusTimerAbility::SendStatusTimer(Hero, VillanASC, ZLItemStatus::GetTag(Status), Duration);
		}
		ZLKitCue::Play(VillanASC, FrozenCue, Villan->GetActorLocation(), (Villan->GetActorLocation() - Origin).GetSafeNormal2D().Rotation());
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, FreezeRadius, 24, Frozen.Num() > 0 ? FColor::Green : FColor::Red, false, 2.0f);
	}
#endif
}

void UZL_KaiKoa_AbsoluteZero::OnFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_KaiKoa_AbsoluteZero::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The client reaches its end first; replicating it would end the server's instance before the freeze.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
