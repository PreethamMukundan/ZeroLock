// Copyright Preetham Mukundan (C) 2026


#include "Doorman/ZL_Doorman_Doorway.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Doorman/ZL_Doorman_Door.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_Doorman_Doorway::UZL_Doorman_Doorway()
{
	// Door placement comes from the client's target data.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DoorClass = AZL_Doorman_Door::StaticClass();
	TargetActorClass = AZL_GATA_SurfaceTrace::StaticClass();

	DoorClearance.bEnabled = true;
	DoorClearance.FrontClearance = 60.0f;
	DoorClearance.bRequireFlushSurface = true;
	DoorClearance.MaxSurfaceGap = 10.0f;
	DoorClearance.bRequireGround = true;
	DoorClearance.MaxGroundStep = 15.0f;
}

void UZL_Doorman_Doorway::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo,
                                          const FGameplayEventData* TriggerEventData)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !DoorClass || !TargetActorClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bCleanedUp = false;
	FirstDoor = nullptr;
	SecondDoor = nullptr;
	Phase = EZL_DoorwayPhase::PlacingFirst;

	StartTargeting();
	if (IsActive())
	{
		WaitForPress();
	}
}

void UZL_Doorman_Doorway::StartTargeting()
{
	AActor* Hero = GetAvatarActorFromActorInfo();
	AZL_GATA_SurfaceTrace* TargetActor = GetWorld()->SpawnActor<AZL_GATA_SurfaceTrace>(TargetActorClass, Hero->GetActorTransform());
	if (!TargetActor)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	TargetActor->SetOwner(Hero);
	TargetActor->PrimaryPC = Cast<APlayerController>(GetCurrentActorInfo()->PlayerController.Get());
	TargetActor->SurfaceMode = SurfaceMode;
	TargetActor->MaxRange = MaxPlacementRange;
	TargetActor->ReticleClass = ReticleClass;
	TargetActor->bDebug = bDrawTargetingDebug;

	// Aim shows the door itself (frame-sized box) where it would go, green when it fits.
	TargetActor->PreviewBox = DoorClass->GetDefaultObject<AZL_Doorman_Door>()->GetLocalBounds();
	TargetActor->PlacementCheck = [WeakThis = TWeakObjectPtr<UZL_Doorman_Doorway>(this)](const FHitResult& Hit, FTransform& OutPlacement)
	{
		return WeakThis.IsValid() && WeakThis->GetDoorTransform(Hit, OutPlacement);
	};
	if (Phase == EZL_DoorwayPhase::PlacingSecond)
	{
		TargetActor->AnchorLocation = FirstDoorLocation;
		TargetActor->MaxAnchorDistance = MaxDoorDistance.GetValueAtLevel(GetAbilityLevel());
		// Can't put door 2 on door 1. The client only knows the replicated door, so look it up by owner.
		for (TActorIterator<AZL_Doorman_Door> It(GetWorld()); It; ++It)
		{
			if (It->GetOwner() == Hero)
			{
				TargetActor->InvalidActors.Add(*It);
			}
		}
	}

	TargetTask = UAbilityTask_WaitTargetData::WaitTargetDataUsingActor(this, TEXT("DoorwayTargeting"), EGameplayTargetingConfirmation::UserConfirmed, TargetActor);
	TargetTask->ValidData.AddDynamic(this, &UZL_Doorman_Doorway::OnTargetConfirmed);
	TargetTask->Cancelled.AddDynamic(this, &UZL_Doorman_Doorway::OnTargetCancelled);
	TargetTask->ReadyForActivation();
}

void UZL_Doorman_Doorway::StopTargeting()
{
	if (TargetTask)
	{
		TargetTask->ValidData.RemoveDynamic(this, &UZL_Doorman_Doorway::OnTargetConfirmed);
		TargetTask->Cancelled.RemoveDynamic(this, &UZL_Doorman_Doorway::OnTargetCancelled);
		// Ending the task destroys its target actor.
		TargetTask->EndTask();
		TargetTask = nullptr;
	}
}

void UZL_Doorman_Doorway::WaitForPress()
{
	InputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	InputPressTask->OnPress.AddDynamic(this, &UZL_Doorman_Doorway::OnInputPressed);
	InputPressTask->ReadyForActivation();
}

void UZL_Doorman_Doorway::StartTimer(float Duration)
{
	StopTimer();
	TimerTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	TimerTask->OnFinish.AddDynamic(this, &UZL_Doorman_Doorway::OnTimerFinished);
	TimerTask->ReadyForActivation();
}

void UZL_Doorman_Doorway::StopTimer()
{
	if (TimerTask)
	{
		TimerTask->OnFinish.RemoveDynamic(this, &UZL_Doorman_Doorway::OnTimerFinished);
		TimerTask->EndTask();
		TimerTask = nullptr;
	}
}

bool UZL_Doorman_Doorway::GetDoorTransform(const FHitResult& Hit, FTransform& OutTransform) const
{
	// Doors stand upright, facing out of the wall.
	const FVector Normal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0f).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return false;
	}

	FVector Location = Hit.ImpactPoint;
	bool bHasFloor = true;
	if (bSnapDoorToFloor)
	{
		const float DoorHeight = DoorClass->GetDefaultObject<AZL_Doorman_Door>()->GetDoorHeight();
		const FVector TraceStart = Location + Normal * 30.0f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DoorwayFloor), false, GetAvatarActorFromActorInfo());
		FHitResult FloorHit;
		bHasFloor = GetWorld()->LineTraceSingleByObjectType(FloorHit, TraceStart, TraceStart - FVector::UpVector * DoorHeight * 2.0f, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		if (bHasFloor)
		{
			Location.Z = FloorHit.ImpactPoint.Z + DoorHeight * 0.5f;
		}
	}

	OutTransform = FTransform(FRotationMatrix::MakeFromXZ(Normal, FVector::UpVector).ToQuat(), Location);
	// The door doesn't clip anything (DoorClearance; same check on the client's aim and the server's re-check).
	return bHasFloor && DoorClearance.IsClear(GetWorld(), OutTransform, DoorClass->GetDefaultObject<AZL_Doorman_Door>()->GetLocalBounds(), GetAvatarActorFromActorInfo());
}

AZL_Doorman_Door* UZL_Doorman_Doorway::SpawnDoor(const FTransform& DoorTransform) const
{
	APawn* Hero = Cast<APawn>(GetAvatarActorFromActorInfo());
	FActorSpawnParameters Params;
	Params.Owner = Hero;
	Params.Instigator = Hero;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<AZL_Doorman_Door>(DoorClass, DoorTransform, Params);
}

void UZL_Doorman_Doorway::OnTargetConfirmed(const FGameplayAbilityTargetDataHandle& Data)
{
	// WaitTargetData ends itself after valid data.
	TargetTask = nullptr;

	FTransform DoorTransform;
	if (!UAbilitySystemBlueprintLibrary::TargetDataHasHitResult(Data, 0)
		|| !GetDoorTransform(UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Data, 0), DoorTransform))
	{
		OnTargetCancelled(Data);
		return;
	}

	const bool bAuthority = HasAuthority(&CurrentActivationInfo);
	UE_LOG(LogTemp, Log, TEXT("Doorway: placing door %d at %s (%s)"), Phase == EZL_DoorwayPhase::PlacingFirst ? 1 : 2,
		*DoorTransform.GetLocation().ToString(), bAuthority ? TEXT("server") : TEXT("client"));

	if (Phase == EZL_DoorwayPhase::PlacingFirst)
	{
		FirstDoorLocation = DoorTransform.GetLocation();
		if (bAuthority)
		{
			FirstDoor = SpawnDoor(DoorTransform);
		}
		// No time limit to place door 2.
		Phase = EZL_DoorwayPhase::WaitingSecond;
		return;
	}

	if (Phase == EZL_DoorwayPhase::PlacingSecond)
	{
		if (bAuthority)
		{
			// The location comes from the client: check the distance again (small slack for the floor snap).
			const float MaxDistance = MaxDoorDistance.GetValueAtLevel(GetAbilityLevel()) + 250.0f;
			if (!IsValid(FirstDoor) || FVector::DistSquared(FirstDoorLocation, DoorTransform.GetLocation()) > FMath::Square(MaxDistance))
			{
				Phase = EZL_DoorwayPhase::WaitingSecond;
				return;
			}
			SecondDoor = SpawnDoor(DoorTransform);
			AZL_Doorman_Door::LinkDoors(FirstDoor, SecondDoor);
		}
		Phase = EZL_DoorwayPhase::Open;
		StartTimer(DoorDuration.GetValueAtLevel(GetAbilityLevel()));
	}
}

void UZL_Doorman_Doorway::OnTargetCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	TargetTask = nullptr;

	if (Phase == EZL_DoorwayPhase::PlacingFirst)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}
	if (Phase == EZL_DoorwayPhase::PlacingSecond)
	{
		Phase = EZL_DoorwayPhase::WaitingSecond;
	}
}

void UZL_Doorman_Doorway::OnInputPressed(float TimeWaited)
{
	InputPressTask = nullptr;

	switch (Phase)
	{
	case EZL_DoorwayPhase::PlacingFirst:
		// Changed their mind before placing anything: free cancel.
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;

	case EZL_DoorwayPhase::WaitingSecond:
		Phase = EZL_DoorwayPhase::PlacingSecond;
		StartTargeting();
		break;

	case EZL_DoorwayPhase::PlacingSecond:
		StopTargeting();
		Phase = EZL_DoorwayPhase::WaitingSecond;
		break;

	case EZL_DoorwayPhase::Open:
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		return;
	}

	if (IsActive())
	{
		WaitForPress();
	}
}

void UZL_Doorman_Doorway::OnTimerFinished()
{
	TimerTask = nullptr;
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UZL_Doorman_Doorway::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bCleanedUp)
	{
		bCleanedUp = true;
		UE_LOG(LogTemp, Log, TEXT("Doorway: end (phase %d, cancelled %d, %s)"), static_cast<int32>(Phase), bWasCancelled ? 1 : 0,
			HasAuthority(&ActivationInfo) ? TEXT("server") : TEXT("client"));

		StopTargeting();
		StopTimer();
		if (InputPressTask)
		{
			InputPressTask->OnPress.RemoveDynamic(this, &UZL_Doorman_Doorway::OnInputPressed);
			InputPressTask->EndTask();
			InputPressTask = nullptr;
		}

		if (IsValid(FirstDoor))
		{
			FirstDoor->Destroy();
		}
		if (IsValid(SecondDoor))
		{
			SecondDoor->Destroy();
		}
		FirstDoor = nullptr;
		SecondDoor = nullptr;

		// The cooldown starts when the doors close; cancelling the first aim is free.
		if (Phase != EZL_DoorwayPhase::PlacingFirst)
		{
			CommitAbility(Handle, ActorInfo, ActivationInfo);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
