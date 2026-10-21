// Copyright Preetham Mukundan (C) 2026


#include "Mover/ZeroInstantMovementEffects.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

bool FZeroTeleportWithVelocityEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!Super::ApplyMovementEffect(ApplyEffectParams, OutputState))
	{
		return false;
	}

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	OutputSyncState.SetTransforms_WorldSpace(OutputSyncState.GetLocation_WorldSpace(),
	                                         OutputSyncState.GetOrientation_WorldSpace(),
	                                         Velocity,
	                                         OutputSyncState.GetAngularVelocityDegrees_WorldSpace(),
	                                         nullptr);
	ApplyEffectParams.UpdatedComponent->ComponentVelocity = Velocity;
	return true;
}

void FZeroTeleportWithVelocityEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << Velocity;
}
