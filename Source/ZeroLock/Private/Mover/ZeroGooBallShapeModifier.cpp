// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroGooBallShapeModifier.h"

#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ZeroGooBallShapeModifier)

FZeroGooBallShapeModifier::FZeroGooBallShapeModifier()
{
	// Lasts until cancelled.
	DurationMs = -1.0f;
}

void FZeroGooBallShapeModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	SetCapsule(MoverComp, BallRadius, BallRadius);
}

void FZeroGooBallShapeModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (const UCapsuleComponent* Original = UMovementUtils::GetOriginalComponentType<UCapsuleComponent>(MoverComp->GetOwner()))
	{
		SetCapsule(MoverComp, Original->GetUnscaledCapsuleRadius(), Original->GetUnscaledCapsuleHalfHeight());
	}
}

void FZeroGooBallShapeModifier::SetCapsule(UMoverComponent* MoverComp, float NewRadius, float NewHalfHeight)
{
	UCapsuleComponent* Capsule = MoverComp ? Cast<UCapsuleComponent>(MoverComp->GetUpdatedComponent()) : nullptr;
	if (!Capsule)
	{
		return;
	}
	const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	if (FMath::IsNearlyEqual(OldHalfHeight, NewHalfHeight) && FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), NewRadius))
	{
		return;
	}
	Capsule->SetCapsuleSize(NewRadius, NewHalfHeight, false);

	// Keep the bottom in place (the stance modifier does the same with a teleport effect, so it's part of the sim).
	TSharedPtr<FTeleportEffect> Teleport = MakeShared<FTeleportEffect>();
	Teleport->TargetLocation = MoverComp->GetUpdatedComponentTransform().GetLocation() + MoverComp->GetUpDirection() * (NewHalfHeight - OldHalfHeight);
	MoverComp->QueueInstantMovementEffect(Teleport);
}

FMovementModifierBase* FZeroGooBallShapeModifier::Clone() const
{
	return new FZeroGooBallShapeModifier(*this);
}

void FZeroGooBallShapeModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << BallRadius;
}

UScriptStruct* FZeroGooBallShapeModifier::GetScriptStruct() const
{
	return FZeroGooBallShapeModifier::StaticStruct();
}

FString FZeroGooBallShapeModifier::ToSimpleString() const
{
	return FString::Printf(TEXT("Goo Ball Shape (r %.0f)"), BallRadius);
}
