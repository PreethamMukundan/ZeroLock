// Copyright Preetham Mukundan (C) 2026

#include "Mover/ZeroDashMove.h"
#include "MoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "Engine/NetSerialization.h"

bool FZeroLayeredMove_Dash::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const float FullSpeed = Velocity.Size();
	float Speed = FullSpeed;

	// Time comes from the sim clock, so a resimulated step produces the same speed.
	const double EaseStartMs = DurationMs - EaseOutMs;
	const double ElapsedMs = TimeStep.BaseSimTimeMs - StartSimTimeMs;
	if (EaseOutMs > 0.0f && ElapsedMs > EaseStartMs)
	{
		const float Alpha = FMath::Clamp(static_cast<float>((ElapsedMs - EaseStartMs) / EaseOutMs), 0.0f, 1.0f);
		Speed = FMath::Lerp(FullSpeed, FMath::Min(ExitSpeed, FullSpeed), Alpha);
	}

	if (CancelOnMoveInputBelowSpeed >= 0.0f)
	{
		const FCharacterDefaultInputs* DefaultInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
		const bool bHasMoveInput = DefaultInputs && !DefaultInputs->GetMoveInput().IsNearlyZero();
		if ((bHasMoveInput && Speed <= CancelOnMoveInputBelowSpeed) || StartState.SyncState.MovementMode != DefaultModeNames::Walking)
		{
			// DurationMs 0 expires the move after this tick, and contributing nothing hands this tick to the mode.
			DurationMs = 0.0f;
			return false;
		}
	}

	OutProposedMove.LinearVelocity = Velocity.GetSafeNormal() * Speed;
	return true;
}

void FZeroLayeredMove_Dash::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 16>(Velocity, Ar);
	Ar << EaseOutMs;
	Ar << ExitSpeed;
	Ar << CancelOnMoveInputBelowSpeed;
}
