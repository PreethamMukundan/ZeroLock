// Copyright Preetham Mukundan (C) 2026


#include "Tempest/ZL_Tempest_AlleyOop.h"

#include "Camera/CameraComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "Kismet/KismetMathLibrary.h"
#include "Tempest/ZL_Tempest_WindBall.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "Xayah/ZL_Xayah_ThrowAbility.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Tempest_AlleyOop::UZL_Tempest_AlleyOop()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ProjectileClass = AZL_Tempest_WindBall::StaticClass();
	AbilityInputID = EGASAbilityInputID::Secondry_Attack;
	Slot = EGameplayAbilitySlot::AbilitySlot1;
	bIsChargedAbility = true;
	MaxChargesConfig = 2.0f;
	TimeBetweenCharges = 0.5f;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_TEMPEST_ALLEYOOP);
	CooldownDuration = 8.0f;
	AbilityName = TEXT("Alley-oop");
	AbilityDescription = TEXT("Throw a wind ball that curves toward your crosshair: aim to steer it. Bursts on impact for 75 spirit damage. 2 charges.");
}

void UZL_Tempest_AlleyOop::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Location = Hero->GetActorLocation();
	FRotator Rotation = Hero->GetActorRotation();
	if (IsLocallyControlled())
	{
		// The remote server gets the real transform from the client's spawn data.
		const UCameraComponent* Camera = Hero->GetFollowCamera();
		const FVector AimDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : GetHeroAimDirection();
		Location = Hero->GetActorLocation() + AimDirection * SpawnForwardOffset;
		Rotation = UKismetMathLibrary::FindLookAtRotation(Location, UZL_Xayah_ThrowAbility::TraceAimPoint(Hero));
	}

	const TSubclassOf<AProjectile> ServerClass = ServerProjectileClass ? ServerProjectileClass : ProjectileClass;
	UZL_Task_SpawnPredictedProjectile* Task = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass, ServerClass, Location, Rotation);
	if (!Task)
	{
		Finish();
		return;
	}
	Task->Success.AddDynamic(this, &UZL_Tempest_AlleyOop::OnBallSpawned);
	Task->FailedToSpawn.AddDynamic(this, &UZL_Tempest_AlleyOop::OnBallFailed);
	Task->ReadyForActivation();
}

void UZL_Tempest_AlleyOop::OnBallSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_Tempest_WindBall* Ball = Cast<AZL_Tempest_WindBall>(SpawnedProjectile))
	{
		const int32 Level = GetAbilityLevel();
		Ball->InitWindBall(BurstDamage.GetValueAtLevel(Level), BurstRadius.GetValueAtLevel(Level) * GetAbilityRangeScale(), this);
	}
	Finish();
}

void UZL_Tempest_AlleyOop::OnBallFailed(AProjectile* SpawnedProjectile)
{
	Finish();
}

void UZL_Tempest_AlleyOop::Finish()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UZL_Tempest_AlleyOop::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The client finishes spawning first; its end must not cancel the server's task still waiting for spawn data.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
