// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZL_EditorAssetLibrary.generated.h"

class UAnimMontage;

/** Editor helpers for asset edits Python can't reach (unreal.ZL_EditorAssetLibrary in editor scripts, e.g. the Deadlock importer). */
UCLASS()
class ZEROEDITORMODULE_API UZL_EditorAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Puts every track of the montage on SlotName (e.g. "UpperBody") and marks it dirty. False if there is no montage or track. */
	UFUNCTION(BlueprintCallable, Category = "ZeroLock|Editor")
	static bool SetMontageSlot(UAnimMontage* Montage, FName SlotName);
};
