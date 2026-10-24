// Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_BaseConeTargetAbility.h"

#include "GAS/BaseCharAbilitySystemComponent.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_BaseConeTargetAbility::UZL_BaseConeTargetAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	Targetclass = AZL_GATA_ConeSingleTarget::StaticClass();
	TargetConfirmationStyle = EGASTargetConfirmationStyle::Confirm;
	bCancelOnRepress = true;
}

void UZL_BaseConeTargetAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                                const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ConeActor = nullptr;

	if (!Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo()) || !Targetclass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UZL_BaseConeTargetAbility::OnTargetActorSpawned(AGameplayAbilityTargetActor_Trace* TargetActor)
{
	if (AZL_GATA_ConeSingleTarget* Cone = Cast<AZL_GATA_ConeSingleTarget>(TargetActor))
	{
		ConeActor = Cone;
		Cone->MaxRange = Range.GetValueAtLevel(GetAbilityLevel());
		Cone->ConeHalfAngle = ConeHalfAngle;
		Cone->TargetTeam = TargetTeam;
		Cone->bSelfCastWithoutTarget = bSelfCastWithoutTarget;
		Cone->bDebug = bDrawTargetingDebug;
		Cone->TargetCheck = [this](const AZeroLockCharacter* Target) { return IsTargetAllowed(Target); };
	}
}

void UZL_BaseConeTargetAbility::OnTargetRepressed(float TimeWaited)
{
	// The self-cast reaches the server as target data
	AZL_GATA_ConeSingleTarget* Cone = ConeActor.Get();
	if (bSelfCastOnDoubleTap && TimeWaited <= SelfCastWindow && Cone)
	{
		Cone->ConfirmOnSelf();
		return;
	}
	Super::OnTargetRepressed(TimeWaited);
}

AZeroLockCharacter* UZL_BaseConeTargetAbility::GetTargetFromData(const FGameplayAbilityTargetDataHandle& Data) const
{
	if (Data.Num() == 0 || !Data.Get(0))
	{
		return nullptr;
	}
	const TArray<TWeakObjectPtr<AActor>> Actors = Data.Get(0)->GetActors();
	return Actors.Num() > 0 ? Cast<AZeroLockCharacter>(Actors[0].Get()) : nullptr;
}

bool UZL_BaseConeTargetAbility::CanConfirmTarget(const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	AZeroLockCharacter* Target = GetTargetFromData(Data);
	if (!Hero || !Target || !IsTargetAllowed(Target))
	{
		return false;
	}

	if (Target == Hero)
	{
		return AllowsSelfCast() && !Hero->bIsDead;
	}

	// The cone again, with slack: on the server the aim and the target's position lag the client's a little.
	const bool bValid = AZL_GATA_ConeSingleTarget::IsValidConeTarget(Hero, Target, Hero->GetPawnViewLocation(), GetHeroAimDirection(),
		Range.GetValueAtLevel(GetAbilityLevel()) + ServerRangeSlack, ConeHalfAngle + ServerAngleSlack, TargetTeam);
	if (!bValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] %s rejected target %s"), *GetName(), HasAuthority(&CurrentActivationInfo) ? TEXT("server") : TEXT("client"), *Target->GetName());
		return false;
	}

	if (HasAuthority(&CurrentActivationInfo) && !Hero->IsOnSameTeam(Target))
	{
		UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
		if (!HeroASC || HeroASC->IsUntouchable(Target->GetMyAbilitySystemComp()))
		{
			return false;
		}
	}
	return true;
}

void UZL_BaseConeTargetAbility::AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data)
{
	ConeTargetConfirmed(GetTargetFromData(Data), Data);
}

void UZL_BaseConeTargetAbility::ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
