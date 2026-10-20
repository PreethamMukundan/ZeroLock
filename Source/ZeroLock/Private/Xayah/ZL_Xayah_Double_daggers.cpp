// Copyright Preetham Mukundan (C) 2026


#include "Xayah/ZL_Xayah_Double_daggers.h"

#include "Camera/CameraComponent.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Xayah/ZL_Xayah_projectile.h"
#include "ZeroLock/ZeroLockCharacter.h"


void UZL_Xayah_Double_daggers::OnAnimationPointTrigger()
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
			FRotator RotNew = Rotation + FRotator(0, -2.5, 0);
			Fire(Hero,Location,RotNew,SpawnParameters);
			RotNew = Rotation + FRotator(0, 2.5, 0);
			Fire(Hero,Location,RotNew,SpawnParameters);
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	
}

void UZL_Xayah_Double_daggers::Fire(AZeroLockCharacter* Hero, FVector Location, FRotator Rotation,
	FActorSpawnParameters SpawnParm)
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (Hero)
		{
			Hero->GetMyAbilitySystemComp()->ApplyGameplayEffectWithStacks(Hero->GetMyAbilitySystemComp(),FeatherDownClassClass,1,1);
		}
	}
	if (!GetCurrentActivationInfo().ActivationMode == EGameplayAbilityActivationMode::Authority)
	{
		return;
	}
	AZL_Xayah_projectile* proj =GetWorld()->SpawnActor<AZL_Xayah_projectile>(ProjectileClass,Location,Rotation,SpawnParm);
	if (proj)
	{
		proj->SetOwner(Hero);
		//proj->SetAutoPull(true);
		proj->OwnerCharacter = Hero;
	}
}

void UZL_Xayah_Double_daggers::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                          bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bWasCancelled)
	{
		if (HasAuthority(&CurrentActivationInfo))
		{
			AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
			if (Hero)
			{
				Hero->GetMyAbilitySystemComp()->ApplyGameplayEffectWithStacks(Hero->GetMyAbilitySystemComp(),FeatherDownClassClass,1,3);
			}
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
