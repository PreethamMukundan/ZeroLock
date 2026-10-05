// Copyright Preetham Mukundan (C) 2026


#include "Pocket/ZL_Pocket_Affliction.h"

#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/ZL_StatusTimerAbility.h"
#include "Pocket/ZL_Pocket_AfflictionEffect.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Pocket_Affliction::UZL_Pocket_Affliction()
{
	AfflictionEffect = UZL_Pocket_AfflictionEffect::StaticClass();
}

void UZL_Pocket_Affliction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                            const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bHasAfflicted = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UZL_Pocket_Affliction::OnAnimationPointTrigger()
{
	AfflictNearbyEnemies();
	Super::OnAnimationPointTrigger();
}

void UZL_Pocket_Affliction::OnAnimationCompleted()
{
	AfflictNearbyEnemies();
	Super::OnAnimationCompleted();
}

void UZL_Pocket_Affliction::AfflictNearbyEnemies()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (bHasAfflicted || !Hero)
	{
		return;
	}
	bHasAfflicted = true;

	TSet<AZeroLockCharacter*> Villans;
	FindTargets(Hero, Villans);

	if (HasAuthority(&CurrentActivationInfo))
	{
		for (AZeroLockCharacter* Villan : Villans)
		{
			ApplyAffliction(Hero, Villan);
		}
	}

	if (bDrawDebugAffliction && (HasAuthority(&CurrentActivationInfo) || IsLocallyControlled()))
	{
		DrawDebugSphere(GetWorld(), Hero->GetActorLocation(), Radius.GetValueAtLevel(GetAbilityLevel()), 24, Villans.Num() > 0 ? FColor::Green : FColor::Red, false, 2.0f);
	}
}

void UZL_Pocket_Affliction::FindTargets(AZeroLockCharacter* Hero, TSet<AZeroLockCharacter*>& OutVillans) const
{
	FCollisionQueryParams QueryParams(FName(TEXT("PocketAffliction")), false, Hero);

	TArray<FOverlapResult> Overlaps;
	// Heroes use the Hero object type (ECC_GameTraceChannel1), and the Hero profile ignores the Hero trace channel, so query by object type.
	GetWorld()->OverlapMultiByObjectType(Overlaps, Hero->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel1), FCollisionShape::MakeSphere(Radius.GetValueAtLevel(GetAbilityLevel())), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AZeroLockCharacter* Villan = Cast<AZeroLockCharacter>(Overlap.GetActor());
		if (IsValid(Villan) && Villan != Hero && !Hero->IsOnSameTeam(Villan) && Villan->GetMyAbilitySystemComp())
		{
			OutVillans.Add(Villan);
		}
	}
}

void UZL_Pocket_Affliction::ApplyAffliction(AZeroLockCharacter* Hero, AZeroLockCharacter* Villan) const
{
	UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
	UBaseCharAbilitySystemComponent* VillanASC = Villan->GetMyAbilitySystemComp();
	if (!HeroASC || !AfflictionEffect || HeroASC->IsUntouchable(VillanASC))
	{
		return;
	}

	const int32 Level = GetAbilityLevel();
	const float AfflictionDuration = Duration.GetValueAtLevel(Level);

	FGameplayEffectContextHandle Context = HeroASC->MakeEffectContext();
	Context.AddSourceObject(Hero);

	FGameplayEffectSpecHandle SpecHandle = HeroASC->MakeOutgoingSpec(AfflictionEffect, Level, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	Spec.SetSetByCallerMagnitude(ZerolockGameplayTagsForBinding::TAG_SETBYCALLER_SPIRIT, DamagePerTick.GetValueAtLevel(Level));
	Spec.SetDuration(AfflictionDuration, true);
	Spec.Period = TickInterval;
	Spec.AddDynamicAssetTag(ZerolockGameplayTagsForBinding::TAG_DAMAGE_NONLETHAL);
	Spec.DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_AFFLICTED);
	if (Level >= HealBlockMinLevel)
	{
		Spec.DynamicGrantedTags.AddTag(ZerolockGameplayTagsForBinding::TAG_STATUS_HEAL_BLOCKED);
	}

	// Recasting restarts the affliction rather than running two DOTs at once.
	VillanASC->RemoveActiveGameplayEffectBySourceEffect(AfflictionEffect, HeroASC);
	HeroASC->ApplyGameplayEffectSpecToTarget(Spec, VillanASC);

	UZL_StatusTimerAbility::SendStatusTimer(Hero, VillanASC, ZerolockGameplayTagsForBinding::TAG_STATUS_AFFLICTED, AfflictionDuration);
}
