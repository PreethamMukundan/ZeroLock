// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_LuggageCart.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Doorman/ZL_Projectile_Doorman_LuggageCart.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Doorman_LuggageCart::UZL_Doorman_LuggageCart()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UZL_Doorman_LuggageCart::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                              const FGameplayAbilityActorInfo* ActorInfo,
                                              const FGameplayAbilityActivationInfo ActivationInfo,
                                              const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (CastMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, CastMontage, 1.0f, NAME_None, false);
		MontageTask->ReadyForActivation();
	}

	// The cart only uses the yaw of the aim.
	FVector Heading = GetHeroAimDirection();
	Heading.Z = 0.0f;
	Heading = Heading.GetSafeNormal();
	if (Heading.IsNearlyZero())
	{
		Heading = Hero->GetActorForwardVector();
	}

	// Facing a wall: spawn against it instead of inside it (the cart then crashes into it right away).
	FVector SpawnLocation = Hero->GetActorLocation() + Heading * SpawnForwardOffset;
	const float CartRadius = ProjectileClass->GetDefaultObject<AZL_Projectile_Doorman_LuggageCart>()->GetCartRadius();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LuggageCartSpawn), false, Hero);
	FHitResult WallHit;
	if (GetWorld()->SweepSingleByObjectType(WallHit, Hero->GetActorLocation(), SpawnLocation, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(CartRadius), Params) && !WallHit.bStartPenetrating)
	{
		SpawnLocation = WallHit.Location;
	}

	UZL_Task_SpawnPredictedProjectile* SpawnTask = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass,
		ServerProjectileClass ? ServerProjectileClass : ProjectileClass, SpawnLocation, Heading.Rotation());
	if (!SpawnTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SpawnTask->Success.AddDynamic(this, &UZL_Doorman_LuggageCart::OnProjectileSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_Doorman_LuggageCart::OnProjectileFailedToSpawn);
	SpawnTask->ReadyForActivation();
}

void UZL_Doorman_LuggageCart::OnProjectileSpawned(AProjectile* SpawnedProjectile)
{
	AZL_Projectile_Doorman_LuggageCart* Cart = Cast<AZL_Projectile_Doorman_LuggageCart>(SpawnedProjectile);
	if (Cart && !Cart->bIsFakeProjectile)
	{
		Cart->InitCart(SpiritDamage.GetValueAtLevel(GetAbilityLevel()), StunEffect);
	}

	// The cart outlives the ability.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Doorman_LuggageCart::OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}
