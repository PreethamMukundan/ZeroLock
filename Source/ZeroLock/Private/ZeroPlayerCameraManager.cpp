//Copyright Preetham Mukundan (C) 2026


#include "ZeroPlayerCameraManager.h"
#include "ZeroBaseCharacterMovementComp.h"
#include "ZeroLock/ZeroLockCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Mover/ZeroMoverComponent.h"

AZeroPlayerCameraManager::AZeroPlayerCameraManager()
{
}

void AZeroPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	if(AZeroLockCharacter* ZeroCharacter = Cast<AZeroLockCharacter>(GetOwningPlayerController()->GetPawn()))
	{
		UZeroMoverComponent* ZMC = Cast<UZeroMoverComponent>(ZeroCharacter->GetZeroMoverComponent());
		if(!ZMC)
		{
			return;
		}
		FVector TargetCrouchOffset = FVector(0,0,45 - 90);
		FVector Offset = FMath::Lerp(FVector::ZeroVector, TargetCrouchOffset, FMath::Clamp(CrouchBlendTime / CrouchBlendDuration, 0.f, 1.f));

		if (ZMC->IsCrouching())
		{
			CrouchBlendTime = FMath::Clamp(CrouchBlendTime + DeltaTime, 0.f, CrouchBlendDuration);
			Offset -= TargetCrouchOffset;
		}
		else
		{
			CrouchBlendTime = FMath::Clamp(CrouchBlendTime - DeltaTime, 0.f, CrouchBlendDuration);
		}

		OutVT.POV.Location += Offset;
	}
}
