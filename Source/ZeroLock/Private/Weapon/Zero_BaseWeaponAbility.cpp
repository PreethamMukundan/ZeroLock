//Copyright Preetham Mukundan (C) 2026


#include "Weapon/Zero_BaseWeaponAbility.h"

#include "AbilitySystemComponent.h"
#include "ZeroBaseCharacterMovementComp.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Weapon/Zero_BaseProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZero_BaseWeaponAbility::UZero_BaseWeaponAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

	FGameplayTag ReloadTag = FGameplayTag::RequestGameplayTag(FName("ZeroLock.Weapon.Reloading"),false);
	ActivationBlockedTags.AddTag(ReloadTag);
}

void UZero_BaseWeaponAbility::Fire()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !ProjectileClass)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	FVector EyeLocation;
	FRotator IgnoredStaleRotation;
	Hero->GetActorEyesViewPoint(EyeLocation, IgnoredStaleRotation);


	FRotator EyeRotation = Hero->GetSyncedAimRotation();
	FVector AimDir = EyeRotation.Vector();

	FVector SpawnLocation = Hero->GetActorLocation() + (AimDir * 100.0f);

	FVector TraceStart = EyeLocation;
	FVector TraceEnd = TraceStart + (AimDir * 10000.0f);
    
    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Hero);
    
    FRotator ProjRotation = EyeRotation;
	
    if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Pawn, QueryParams))
    {
        ProjRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, Hit.ImpactPoint);
    }
    else
    {
        ProjRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, TraceEnd);
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = Hero;
    SpawnParameters.Instigator = Hero->GetInstigator();
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Cyan, false, 10.0f);
    
    AZero_BaseProjectile* Proj = GetWorld()->SpawnActor<AZero_BaseProjectile>(ProjectileClass, SpawnLocation, ProjRotation, SpawnParameters);
    if (Proj)
    {
         Proj->SetOwner(Hero);
         Proj->OwnerCharacter = Hero;
    }

    CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZero_BaseWeaponAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	
	if (GetCurrentActivationInfo().ActivationMode == EGameplayAbilityActivationMode::Authority)
	{
		Fire();
	}

	
}



