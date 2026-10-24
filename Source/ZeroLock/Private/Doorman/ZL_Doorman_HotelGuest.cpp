// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_HotelGuest.h"

#include "AbilitySystemComponent.h"
#include "Doorman/ZL_Doorman_Hotel.h"
#include "Doorman/ZL_Doorman_HotelGuestEffect.h"
#include "Doorman/ZL_Doorman_HotelPlaceholder.h"
#include "GAS/ZL_GameplayTags.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Doorman_HotelGuest::UZL_Doorman_HotelGuest()
{
	GuestEffect = UZL_Doorman_HotelGuestEffect::StaticClass();
	PlaceholderClass = AZL_Doorman_HotelPlaceholder::StaticClass();
	HotelClass = AZL_Doorman_Hotel::StaticClass();
}

void UZL_Doorman_HotelGuest::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                             const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	GuestRef = nullptr;
	bHasEnded = false;

	// Find or spawn the hotel now, so a freshly spawned one has replicated to the guest by the time they're sent.
	if (HasAuthority(&ActivationInfo))
	{
		HotelRef = AZL_Doorman_Hotel::GetOrSpawnHotel(GetWorld(), HotelClass, HotelSpawnLocation);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
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

void UZL_Doorman_HotelGuest::ConeTargetConfirmed(AZeroLockCharacter* Target, const FGameplayAbilityTargetDataHandle& Data)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
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
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
