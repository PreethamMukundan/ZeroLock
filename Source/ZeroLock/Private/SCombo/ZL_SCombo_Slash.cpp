// Copyright Preetham Mukundan (C) 2026


#include "SCombo/ZL_SCombo_Slash.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Debug/ZL_DebugDraw.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "SCombo/ZL_SCombo_Combo.h"
#include "SCombo/ZL_SCombo_Projectile.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace
{
	constexpr EAbilityGenericReplicatedEvent::Type SlashEvent = EAbilityGenericReplicatedEvent::GameCustom1;
	constexpr EAbilityGenericReplicatedEvent::Type ProjectileEvent = EAbilityGenericReplicatedEvent::GameCustom2;
}

UZL_SCombo_Slash::UZL_SCombo_Slash()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ProjectileClass = AZL_SCombo_Projectile::StaticClass();
}

void UZL_SCombo_Slash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Hero || !ASC || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
	if (!IsLocallyControlled())
	{
		// Remote server: wait for the owner's choice. It may already be here.
		SlashEventHandle = ASC->AbilityReplicatedEventDelegate(SlashEvent, Handle, ActivationKey).AddUObject(this, &UZL_SCombo_Slash::OnClientChoseSlash);
		ProjectileEventHandle = ASC->AbilityReplicatedEventDelegate(ProjectileEvent, Handle, ActivationKey).AddUObject(this, &UZL_SCombo_Slash::OnClientChoseProjectile);
		if (!ASC->CallReplicatedEventDelegateIfSet(SlashEvent, Handle, ActivationKey))
		{
			ASC->CallReplicatedEventDelegateIfSet(ProjectileEvent, Handle, ActivationKey);
		}
		return;
	}

	TArray<AZeroLockCharacter*> Villans;
	FindSlashTargets(Hero, 0.0f, 0.0f, Villans);
	const bool bSlash = !Villans.IsEmpty();
	if (!HasAuthority(&ActivationInfo))
	{
		ASC->ServerSetReplicatedEvent(bSlash ? SlashEvent : ProjectileEvent, Handle, ActivationKey, ASC->ScopedPredictionKey);
	}

	if (bSlash)
	{
		DoSlash();
	}
	else
	{
		DoProjectile();
	}
}

void UZL_SCombo_Slash::OnClientChoseSlash()
{
	StopWaitingForClient();
	DoSlash();
}

void UZL_SCombo_Slash::OnClientChoseProjectile()
{
	StopWaitingForClient();
	DoProjectile();
}

void UZL_SCombo_Slash::StopWaitingForClient()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}
	const FPredictionKey ActivationKey = GetCurrentActivationInfo().GetActivationPredictionKey();
	if (SlashEventHandle.IsValid())
	{
		ASC->AbilityReplicatedEventDelegate(SlashEvent, CurrentSpecHandle, ActivationKey).Remove(SlashEventHandle);
		ASC->ConsumeGenericReplicatedEvent(SlashEvent, CurrentSpecHandle, ActivationKey);
		SlashEventHandle.Reset();
	}
	if (ProjectileEventHandle.IsValid())
	{
		ASC->AbilityReplicatedEventDelegate(ProjectileEvent, CurrentSpecHandle, ActivationKey).Remove(ProjectileEventHandle);
		ASC->ConsumeGenericReplicatedEvent(ProjectileEvent, CurrentSpecHandle, ActivationKey);
		ProjectileEventHandle.Reset();
	}
}

void UZL_SCombo_Slash::FindSlashTargets(AZeroLockCharacter* Hero, float RangeSlack, float AngleSlack, TArray<AZeroLockCharacter*>& OutVillans) const
{
	const float Range = SlashRange.GetValueAtLevel(GetAbilityLevel()) * GetAbilityRangeScale() + RangeSlack;
	ZLSCombo::FindEnemiesInRadius(Hero, Range, OutVillans);

	FVector Forward = GetHeroAimDirection().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = Hero->GetActorForwardVector();
	}
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Min(SlashHalfAngle + AngleSlack, 180.0f)));
	const FVector Center = Hero->GetActorLocation();
	OutVillans.RemoveAll([&](const AZeroLockCharacter* Villan)
	{
		return FVector::DotProduct((Villan->GetActorLocation() - Center).GetSafeNormal2D(), Forward) < MinDot;
	});

#if ENABLE_DRAW_DEBUG
	if (bDrawSlashDebug && HasAuthority(&CurrentActivationInfo))
	{
		const float Angle = FMath::DegreesToRadians(SlashHalfAngle + AngleSlack);
		ZLDebugDraw::Cone(GetWorld(), Center, Forward, Range, Angle, 0.05f, OutVillans.IsEmpty() ? FColor::Yellow : FColor::Red, 2.0f);
		for (const AZeroLockCharacter* Villan : OutVillans)
		{
			ZLDebugDraw::Sphere(GetWorld(), Villan->GetActorLocation(), 60.0f, FColor::Red, 2.0f);
		}
	}
#endif
}

void UZL_SCombo_Slash::DoSlash()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && SlashMontage)
	{
		ASC->PlayMontage(this, GetCurrentActivationInfo(), SlashMontage, 1.0f);
	}

	UBaseCharAbilitySystemComponent* HeroASC = Hero ? Hero->GetMyAbilitySystemComp() : nullptr;
	if (HeroASC && HasAuthority(&CurrentActivationInfo))
	{
		TArray<AZeroLockCharacter*> Villans;
		const bool bRemote = !IsLocallyControlled();
		FindSlashTargets(Hero, bRemote ? ServerRangeSlack : 0.0f, bRemote ? ServerAngleSlack : 0.0f, Villans);

		const float Damage = SlashDamage.GetValueAtLevel(GetAbilityLevel());
		for (AZeroLockCharacter* Villan : Villans)
		{
			HeroASC->ApplyWeaponDamage(Villan->GetMyAbilitySystemComp(), Damage, this);
		}
	}
	FinishSlashAbility(false);
}

void UZL_SCombo_Slash::DoProjectile()
{
	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Hero)
	{
		FinishSlashAbility(true);
		return;
	}
	if (ASC && ProjectileMontage)
	{
		ASC->PlayMontage(this, GetCurrentActivationInfo(), ProjectileMontage, 1.0f);
	}

	FVector SpawnLocation = Hero->GetActorLocation();
	FRotator SpawnRotation = Hero->GetActorRotation();
	const UCameraComponent* Camera = Hero->GetFollowCamera();
	if (IsLocallyControlled())
	{
		// A remote server takes the transform from the client, so this only matters on the owner's machine.
		const FVector AimDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : GetHeroAimDirection();
		SpawnLocation += AimDirection * SpawnForwardOffset;
		SpawnRotation = AimDirection.Rotation();

		if (Camera)
		{
			// Aim at whatever is under the crosshair.
			const FVector TraceStart = Camera->GetComponentLocation();
			const FVector TraceEnd = TraceStart + AimDirection * 10000.0f;
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZL_SCombo_SlashAim), false, Hero);
			FHitResult Hit;
			const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) ? Hit.ImpactPoint : TraceEnd;
			SpawnRotation = (AimPoint - SpawnLocation).Rotation();
		}
	}

	UZL_Task_SpawnPredictedProjectile* SpawnTask = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass,
		ServerProjectileClass ? ServerProjectileClass : ProjectileClass, SpawnLocation, SpawnRotation);
	if (!SpawnTask)
	{
		FinishSlashAbility(true);
		return;
	}
	SpawnTask->Success.AddDynamic(this, &UZL_SCombo_Slash::OnProjectileSpawned);
	SpawnTask->FailedToSpawn.AddDynamic(this, &UZL_SCombo_Slash::OnProjectileFailedToSpawn);
	SpawnTask->ReadyForActivation();
}

void UZL_SCombo_Slash::OnProjectileSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_SCombo_Projectile* Projectile = Cast<AZL_SCombo_Projectile>(SpawnedProjectile))
	{
		Projectile->SetSpiritDamage(ProjectileDamage.GetValueAtLevel(GetAbilityLevel()));
	}
	FinishSlashAbility(false);
}

void UZL_SCombo_Slash::OnProjectileFailedToSpawn(AProjectile* SpawnedProjectile)
{
	FinishSlashAbility(true);
}

void UZL_SCombo_Slash::FinishSlashAbility(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UZL_SCombo_Slash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                  const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The client finishes first, and replicating that would cancel the server's instance before it deals the damage or
	// spawns the projectile. The server ends its own. Real cancels still reach it: CancelAbility replicates on its own.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	StopWaitingForClient();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
