// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_HotelGuest.h"

#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Doorman/ZL_Doorman_Hotel.h"
#include "Doorman/ZL_Doorman_HotelGuestEffect.h"
#include "Doorman/ZL_Doorman_HotelPlaceholder.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "GAS/TargetActors/ZL_GATA_ConeSingleTarget.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Doorman_HotelGuest::UZL_Doorman_HotelGuest()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	Targetclass = AZL_GATA_ConeSingleTarget::StaticClass();
	TargetConfirmationStyle = EGASTargetConfirmationStyle::Confirm;
	GuestEffect = UZL_Doorman_HotelGuestEffect::StaticClass();
	PlaceholderClass = AZL_Doorman_HotelPlaceholder::StaticClass();
	HotelClass = AZL_Doorman_Hotel::StaticClass();
}

void UZL_Doorman_HotelGuest::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                             const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	GuestRef = nullptr;
	bTargetConfirmed = false;
	bHasEnded = false;

	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !Targetclass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Find or spawn the hotel now, so a freshly spawned one has replicated to the guest by the time they're sent.
	if (HasAuthority(&ActivationInfo))
	{
		HotelRef = AZL_Doorman_Hotel::GetOrSpawnHotel(GetWorld(), HotelClass, HotelSpawnLocation);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	RepressTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	RepressTask->OnPress.AddDynamic(this, &UZL_Doorman_HotelGuest::OnRepressed);
	RepressTask->ReadyForActivation();
}

void UZL_Doorman_HotelGuest::OnTargetActorSpawned(AGameplayAbilityTargetActor_Trace* TargetActor)
{
	if (AZL_GATA_ConeSingleTarget* Cone = Cast<AZL_GATA_ConeSingleTarget>(TargetActor))
	{
		Cone->MaxRange = Range.GetValueAtLevel(GetAbilityLevel());
		Cone->ConeHalfAngle = ConeHalfAngle;
		Cone->bDebug = bDrawTargetingDebug;
		Cone->TargetCheck = [this](const AZeroLockCharacter* Target) { return IsTargetAllowed(Target); };
	}
}

void UZL_Doorman_HotelGuest::OnRepressed(float TimeWaited)
{
	if (!bTargetConfirmed)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

AZeroLockCharacter* UZL_Doorman_HotelGuest::GetTargetFromData(const FGameplayAbilityTargetDataHandle& Data) const
{
	if (Data.Num() == 0 || !Data.Get(0))
	{
		return nullptr;
	}
	const TArray<TWeakObjectPtr<AActor>> Actors = Data.Get(0)->GetActors();
	return Actors.Num() > 0 ? Cast<AZeroLockCharacter>(Actors[0].Get()) : nullptr;
}

bool UZL_Doorman_HotelGuest::IsTargetAllowed(const AZeroLockCharacter* Target) const
{
	const UAbilitySystemComponent* TargetASC = Target ? Target->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC || TargetASC->HasMatchingGameplayTag(ZerolockGameplayTagsForBinding::TAG_STATUS_HOTEL_GUEST))
	{
		return false;
	}
	const AZL_Doorman_Hotel* Hotel = HotelRef.Get();
	return !Hotel || !Hotel->IsGuest(Target);
}

bool UZL_Doorman_HotelGuest::CanConfirmTarget(const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	AZeroLockCharacter* Target = GetTargetFromData(Data);
	if (!Hero || !Target || !IsTargetAllowed(Target))
	{
		return false;
	}

	// The cone again, with slack: on the server the aim and the target's position lag the client's a little.
	const bool bValid = AZL_GATA_ConeSingleTarget::IsValidConeTarget(Hero, Target, Hero->GetPawnViewLocation(), GetHeroAimDirection(),
		Range.GetValueAtLevel(GetAbilityLevel()) + ServerRangeSlack, ConeHalfAngle + ServerAngleSlack);
	if (!bValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HotelGuest] %s rejected target %s"), HasAuthority(&CurrentActivationInfo) ? TEXT("server") : TEXT("client"), *Target->GetName());
		return false;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		UBaseCharAbilitySystemComponent* HeroASC = Hero->GetMyAbilitySystemComp();
		if (!HeroASC || HeroASC->IsUntouchable(Target->GetMyAbilitySystemComp()))
		{
			return false;
		}
	}
	return true;
}

void UZL_Doorman_HotelGuest::AbilityConfirmedAction(const FGameplayAbilityTargetDataHandle& Data)
{
	bTargetConfirmed = true;
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	AZeroLockCharacter* Target = GetTargetFromData(Data);
	GuestRef = Target;

	// The client waits for the server to end the ability when the guest is out.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	AZL_Doorman_Hotel* Hotel = HotelRef.IsValid() ? HotelRef.Get() : AZL_Doorman_Hotel::GetOrSpawnHotel(GetWorld(), HotelClass, HotelSpawnLocation);
	HotelRef = Hotel;
	if (!Hero || !Target || !Hotel)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Bound before CheckIn: a lethal entry fee checks the guest out inside it.
	StayEndedHandle = Hotel->OnStayEnded.AddUObject(this, &UZL_Doorman_HotelGuest::OnStayEnded);

	const int32 Level = GetAbilityLevel();
	FZL_HotelStayParams Params;
	Params.TimeLimit = TimeLimit.GetValueAtLevel(Level);
	Params.EntryDamage = EntryDamage.GetValueAtLevel(Level);
	Params.LateCheckoutDamage = LateCheckoutDamage.GetValueAtLevel(Level);
	Params.Level = Level;
	Params.GuestEffect = GuestEffect;
	Params.PlaceholderClass = PlaceholderClass;

	if (!Hotel->CheckIn(Hero, Target, Params))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HotelGuest] check-in failed (hotel full or already a guest)"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UZL_Doorman_HotelGuest::OnStayEnded(AZeroLockCharacter* Guest, EZL_HotelCheckout Reason)
{
	if (bHasEnded || (Guest != GuestRef.Get() && GuestRef.IsValid()))
	{
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZL_Doorman_HotelGuest::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bHasEnded)
	{
		bHasEnded = true;
		// Only stop listening: the stay itself belongs to the hotel and carries on.
		if (AZL_Doorman_Hotel* Hotel = HotelRef.Get())
		{
			Hotel->OnStayEnded.Remove(StayEndedHandle);
		}
		StayEndedHandle.Reset();
		if (RepressTask)
		{
			RepressTask->OnPress.RemoveAll(this);
			RepressTask->EndTask();
			RepressTask = nullptr;
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
