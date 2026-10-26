// Copyright Preetham Mukundan (C) 2026


#include "ZL_EditorAssetLibrary.h"

#include "Animation/AnimMontage.h"

bool UZL_EditorAssetLibrary::SetMontageSlot(UAnimMontage* Montage, FName SlotName)
{
	if (!Montage || SlotName.IsNone() || Montage->SlotAnimTracks.IsEmpty())
	{
		return false;
	}
	Montage->Modify();
	for (FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		Track.SlotName = SlotName;
	}
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}
