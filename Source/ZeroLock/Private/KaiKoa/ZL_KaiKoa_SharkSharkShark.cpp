// Copyright Preetham Mukundan (C) 2026


#include "KaiKoa/ZL_KaiKoa_SharkSharkShark.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_GameplayTags.h"
#include "KaiKoa/ZL_KaiKoa_SharkProjectile.h"
#include "Kismet/KismetMathLibrary.h"
#include "Weapon/ZL_Task_SpawnPredictedProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_KaiKoa_SharkSharkShark::UZL_KaiKoa_SharkSharkShark()
{
	// SpawnPredictedProjectile requires a Local Predicted ability.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ProjectileClass = AZL_KaiKoa_SharkProjectile::StaticClass();
	AbilityInputID = EGASAbilityInputID::Secondry_Attack;
	Slot = EGameplayAbilitySlot::AbilitySlot1;
	CooldownTags.AddTag(ZerolockGameplayTagsForBinding::TAG_COOLDOWN_KAIKOA_SHARKSHARKSHARK);
	CooldownDuration = 8.0f;
	AbilityName = TEXT("Shark Shark Shark");
	AbilityDescription = TEXT("Throw three water sharks in a fan. Each one swims through enemies, biting every one it passes for 45 spirit damage.");
}

void UZL_KaiKoa_SharkSharkShark::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Location;
	FRotator Aim;
	GetAim(Hero, Location, Aim);
	ZLKitCue::Play(Hero->GetMyAbilitySystemComp(), CastCue, Hero->GetActorLocation(), Aim);

	PendingSpawns = NumSharks;
	if (!IsLocallyControlled())
	{
		// Remote server: the transforms come from the client's spawn data, one packet per task.
		for (int32 Index = 0; Index < NumSharks && IsActive(); ++Index)
		{
			StartSpawnTask(Hero->GetActorLocation(), Hero->GetActorRotation());
		}
		return;
	}

	const float YawStep = NumSharks > 1 ? FanAngle / (NumSharks - 1) : 0.0f;
	const float FirstYaw = NumSharks > 1 ? -FanAngle * 0.5f : 0.0f;
	for (int32 Index = 0; Index < NumSharks && IsActive(); ++Index)
	{
		StartSpawnTask(Location, Aim + FRotator(0.0f, FirstYaw + YawStep * Index, 0.0f));
	}
}

void UZL_KaiKoa_SharkSharkShark::GetAim(const AZeroLockCharacter* Hero, FVector& OutLocation, FRotator& OutRotation) const
{
	const UCameraComponent* Camera = Hero->GetFollowCamera();
	const FVector AimDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : GetHeroAimDirection();
	OutLocation = Hero->GetActorLocation() + AimDirection * SpawnForwardOffset;
	OutRotation = AimDirection.Rotation();
	if (!Camera)
	{
		return;
	}

	// Converge on whatever the crosshair is on, like the other projectile abilities.
	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector TraceEnd = TraceStart + AimDirection * 10000.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZL_KaiKoa_SharkAim), false, Hero);
	FHitResult Hit;
	const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params) ? Hit.ImpactPoint : TraceEnd;
	OutRotation = UKismetMathLibrary::FindLookAtRotation(OutLocation, AimPoint);
}

void UZL_KaiKoa_SharkSharkShark::StartSpawnTask(const FVector& Location, const FRotator& Rotation)
{
	const TSubclassOf<AProjectile> ServerClass = ServerProjectileClass ? ServerProjectileClass : ProjectileClass;
	UZL_Task_SpawnPredictedProjectile* Task = UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, ProjectileClass, ServerClass, Location, Rotation);
	if (!Task)
	{
		OnSharkFailedToSpawn(nullptr);
		return;
	}
	Task->Success.AddDynamic(this, &UZL_KaiKoa_SharkSharkShark::OnSharkSpawned);
	Task->FailedToSpawn.AddDynamic(this, &UZL_KaiKoa_SharkSharkShark::OnSharkFailedToSpawn);
	Task->ReadyForActivation();
}

void UZL_KaiKoa_SharkSharkShark::OnSharkSpawned(AProjectile* SpawnedProjectile)
{
	if (AZL_KaiKoa_SharkProjectile* Shark = Cast<AZL_KaiKoa_SharkProjectile>(SpawnedProjectile))
	{
		Shark->InitShark(SpiritDamage.GetValueAtLevel(GetAbilityLevel()), this);
	}
	OnSpawnReported();
}

void UZL_KaiKoa_SharkSharkShark::OnSharkFailedToSpawn(AProjectile* SpawnedProjectile)
{
	// One failed shark doesn't cancel the others.
	OnSpawnReported();
}

void UZL_KaiKoa_SharkSharkShark::OnSpawnReported()
{
	if (--PendingSpawns <= 0 && IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UZL_KaiKoa_SharkSharkShark::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	PendingSpawns = 0;
	// The client finishes spawning first; its end must not cancel the server's tasks still waiting for spawn data.
	// The server ends once all of them report.
	if (bReplicateEndAbility && ActorInfo && !ActorInfo->IsNetAuthority())
	{
		bReplicateEndAbility = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
