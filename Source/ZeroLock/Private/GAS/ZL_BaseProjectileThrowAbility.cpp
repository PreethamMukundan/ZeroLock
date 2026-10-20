//Copyright Preetham Mukundan (C) 2026


#include "GAS/ZL_BaseProjectileThrowAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "network/PredictedProjectile.h"
#include "network/RealProjectile.h"
#include "Weapon/Zero_BaseProjectile.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UZL_BaseProjectileThrowAbility::FireProjectile()
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero)
	{
		
		if (ProjectileClass)
		{
				
			// Aim and trace from the eyes, not the follow camera: the server doesn't turn a remote player's camera.
			const FVector AimDir = GetHeroAimDirection();
			FVector Location = Hero->GetActorLocation() + AimDir * 100.0f;
			FRotator Rotation = Hero->GetActorRotation();
			FVector CamTraceStartLocation = Hero->GetPawnViewLocation();
			FVector CamTraceEndLocation = CamTraceStartLocation + AimDir * 10000.0f;
			FHitResult Hit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(Hero);
			Rotation = UKismetMathLibrary::FindLookAtRotation(Location, CamTraceEndLocation);
			
			if (GetWorld()->LineTraceSingleByChannel(Hit,CamTraceStartLocation,CamTraceEndLocation,ECC_Pawn,QueryParams))
			{
				FVector HitLoc = Hit.ImpactPoint;
				Rotation = UKismetMathLibrary::FindLookAtRotation(Location, HitLoc);
			}
				
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = Hero;
			SpawnParameters.Instigator = Hero->GetInstigator();
			SpawnParameters.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			//if (GetActorInfo().IsLocallyControlled() && PredictedProjectileClass)
			//{
			//	PredictedProjectile = GetAvatarActorFromActorInfo()->GetWorld()->SpawnActor<APredictedProjectile>(PredictedProjectileClass,Location,Rotation,SpawnParameters);
			//	
			//}

			// 🔹 2. Ask Server to spawn real projectile
			if (GetActorInfo().IsNetAuthority())
			{
				Server_SpawnProjectile(Location,Rotation,PredictedProjectile);
			}
		}
	}
	
}

void UZL_BaseProjectileThrowAbility::OnEventRecived(FGameplayEventData Payload)
{
	//CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	
}

void UZL_BaseProjectileThrowAbility::MissOnEventRecived(FGameplayEventData Payload)
{
	ZLOG("EventMiss");
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}






void UZL_BaseProjectileThrowAbility::Server_SpawnProjectile_Implementation(FVector const& Location,
	FRotator const& Rotation, APredictedProjectile* PredProj)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (Hero)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Hero;
		SpawnParameters.Instigator = Hero->GetInstigator();
		SpawnParameters.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (RealProjectileClass)
		{
			ARealProjectile* RealProj = GetWorld()->SpawnActor<ARealProjectile>(RealProjectileClass, Location,Rotation, SpawnParameters);
			if (RealProj)
			{
				RealProj->OwningAbility = this; // store back reference
				RealProj->OwningPredProj = PredProj;
			}
		}
	}
}

void UZL_BaseProjectileThrowAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                     const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                     const FGameplayEventData* TriggerEventData)
{
	FireProjectile();
	WaitGameplayEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,ProjectileHitTag);
	WaitGameplayEvent->EventReceived.AddDynamic(this,&UZL_BaseProjectileThrowAbility::OnEventRecived);
	WaitGameplayEvent->ReadyForActivation();

	MissWaitGameplayEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,ProjectileMissTag);
	MissWaitGameplayEvent->EventReceived.AddDynamic(this,&UZL_BaseProjectileThrowAbility::MissOnEventRecived);
	MissWaitGameplayEvent->ReadyForActivation();
}
