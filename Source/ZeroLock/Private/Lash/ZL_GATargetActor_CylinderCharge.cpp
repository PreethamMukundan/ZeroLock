// Copyright Preetham Mukundan (C) 2026


#include "Lash/ZL_GATargetActor_CylinderCharge.h"

#include "Abilities/GameplayAbility.h"
#include "Camera/CameraComponent.h"
#include "Engine/OverlapResult.h"
#include "GAS/BaseGameplayAbility.h"
#include "ZeroLock/ZeroLockCharacter.h"

AZL_GATargetActor_CylinderCharge::AZL_GATargetActor_CylinderCharge()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AZL_GATargetActor_CylinderCharge::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);
	OwningAbility = Ability;
	PrimaryPC = Cast<APlayerController>(Ability->GetActorInfo().PlayerController.Get());
}

FVector AZL_GATargetActor_CylinderCharge::GetCylinderCenter() const
{
	if (!OwningAbility) return GetActorLocation();
	
	AZeroLockCharacter* Avatar = Cast<AZeroLockCharacter>(OwningAbility->GetAvatarActorFromActorInfo());
	if (!Avatar) return GetActorLocation();

	return Avatar->GetActorLocation() + (GetAimDirection() * (CylinderHeight * 0.5f));
}

FVector AZL_GATargetActor_CylinderCharge::GetAimDirection() const
{
	// Same aim on client and server (the follow camera doesn't turn on the server for remote players).
	if (const UBaseGameplayAbility* Ability = Cast<UBaseGameplayAbility>(OwningAbility))
	{
		return Ability->GetHeroAimDirection();
	}
	const AActor* Avatar = OwningAbility ? OwningAbility->GetAvatarActorFromActorInfo() : nullptr;
	return Avatar ? Avatar->GetActorForwardVector() : FVector::ForwardVector;
}

void AZL_GATargetActor_CylinderCharge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (OwningAbility)
	{
		UpdateTargetingData(DeltaSeconds);
		ManageReticles();
	}
}

void AZL_GATargetActor_CylinderCharge::UpdateTargetingData(float DeltaSeconds)
{
	AZeroLockCharacter* Avatar = Cast<AZeroLockCharacter>(OwningAbility->GetAvatarActorFromActorInfo());
	if (!Avatar) return;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);

	TArray<FOverlapResult> Overlaps;
	
	FCollisionShape Capsule = FCollisionShape::MakeCapsule(CylinderRadius, CylinderHeight * 0.5f);
	
	FQuat Rotation = GetAimDirection().ToOrientationQuat();
	DrawDebugCapsule(GetWorld(), GetCylinderCenter(), CylinderHeight * 0.5f, CylinderRadius, Rotation, FColor::Green, false, -1, 0, 2.f);

	GetWorld()->OverlapMultiByChannel(Overlaps, GetCylinderCenter(), Rotation, ECC_Pawn, Capsule, Params);

	TSet<AActor*> CurrentOverlapSet;
	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* OverlappedActor = Result.GetActor();
		if (IsValidTarget(OverlappedActor))
		{
			CurrentOverlapSet.Add(OverlappedActor);
			float& Time = TrackedActors.FindOrAdd(OverlappedActor);
			Time = FMath::Min(Time + DeltaSeconds, RequiredTime);
		}
	}

	// Decay/Cleanup
	TArray<AActor*> Keys;
	TrackedActors.GetKeys(Keys);
	for (AActor* Actor : Keys)
	{
		if (!CurrentOverlapSet.Contains(Actor))
		{
			TrackedActors[Actor] -= DeltaSeconds;
			if (TrackedActors[Actor] <= 0)
			{
				TrackedActors.Remove(Actor);
			}
		}
	}
}

void AZL_GATargetActor_CylinderCharge::ManageReticles()
{
	if (!ReticleClass) return;

	for (auto& Elem : TrackedActors)
	{
		AActor* Target = Elem.Key;
		if (!SpawnedReticles.Contains(Target))
		{
			AGameplayAbilityWorldReticle* NewReticle = GetWorld()->SpawnActor<AGameplayAbilityWorldReticle>(ReticleClass);
			if (NewReticle)
			{
				NewReticle->InitializeReticle(this, PrimaryPC, ReticleParams);
				// AGameplayAbilityWorldReticle uses FaceTarget with a bool, 
				// or we just track it. We'll set the actor as our "targeting source" for the reticle logic.
				NewReticle->SetActorLocation(Target->GetActorLocation());
				SpawnedReticles.Add(Target, NewReticle);
			}
		}
		else
		{
			// Keep reticle attached to the target's position
			SpawnedReticles[Target]->SetActorLocation(Target->GetActorLocation());
		}
	}

	TArray<AActor*> ReticleKeys;
	SpawnedReticles.GetKeys(ReticleKeys);
	for (AActor* Actor : ReticleKeys)
	{
		if (!TrackedActors.Contains(Actor))
		{
			if (SpawnedReticles[Actor]) SpawnedReticles[Actor]->Destroy();
			SpawnedReticles.Remove(Actor);
		}
	}
}

bool AZL_GATargetActor_CylinderCharge::IsValidTarget(AActor* Actor) const
{
	if (!Actor) return false;
	// Use the filter provided by the ability
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(OwningAbility->GetAvatarActorFromActorInfo());
	AZeroLockCharacter* Villian = Cast<AZeroLockCharacter>(Actor);
	if (!Hero || !Villian) return false;
	return (!Hero->IsOnSameTeam(Villian));
	
	
	//return Filter.FilterPassesForActor(Actor);
}

void AZL_GATargetActor_CylinderCharge::ConfirmTargetingAndContinue()
{
	// Corrected: Use OwningAbility instead of SourceAbility
	if (OwningAbility)
	{
		FGameplayAbilityTargetDataHandle Handle;
		TArray<TWeakObjectPtr<AActor>> FinalTargets;

		for (auto& Elem : TrackedActors)
		{
			if (Elem.Value >= RequiredTime)
			{
				FinalTargets.Add(Elem.Key);
			}
		}

		if (FinalTargets.Num() > 0)
		{
			FGameplayAbilityTargetData_ActorArray* ActorData = new FGameplayAbilityTargetData_ActorArray();
			ActorData->TargetActorArray = FinalTargets;
			Handle.Add(ActorData);
		}

		TargetDataReadyDelegate.Broadcast(Handle);
	}
}

void AZL_GATargetActor_CylinderCharge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto& Elem : SpawnedReticles)
	{
		if (Elem.Value) Elem.Value->Destroy();
	}
	SpawnedReticles.Empty();
	Super::EndPlay(EndPlayReason);
}