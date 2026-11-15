// Copyright Preetham Mukundan (C) 2026


#include "UI/OverHead/ZL_OverHeadWidgetComponent.h"

#include "Kismet/GameplayStatics.h"

UZL_OverHeadWidgetComponent::UZL_OverHeadWidgetComponent()
{
	SetWidgetSpace(EWidgetSpace::World);
	SetDrawAtDesiredSize(true);
}

void UZL_OverHeadWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsNetMode(NM_DedicatedServer)) return;

	APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (CamManager)
	{
		FVector CameraLocation = CamManager->GetCameraLocation();
		FVector WidgetLocation = GetComponentLocation();

		// Calculate rotation to face camera
		FRotator LookAtRotation = FRotationMatrix::MakeFromX(CameraLocation - WidgetLocation).Rotator();

			// Only rotate around the Z axis (upright)
			LookAtRotation.Pitch = 0.0f;
			LookAtRotation.Roll = 0.0f;

		SetWorldRotation(LookAtRotation);
	}
}

void UZL_OverHeadWidgetComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!FMath::IsNearlyZero(ExtraHeight))
	{
		AddRelativeLocation(FVector(0.f, 0.f, ExtraHeight));
	}
}
