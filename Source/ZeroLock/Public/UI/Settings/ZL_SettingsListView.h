// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonListView.h"
#include "ZL_SettingsListView.generated.h"

enum class ESettingInteractionType : uint8;
class USettingsBaseRow;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_SettingsListView : public UCommonListView
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TMap<ESettingInteractionType, TSubclassOf<USettingsBaseRow>> SettingSubClassList;

protected:
	virtual TSubclassOf<UUserWidget> GetDesiredEntryClassForItem(UObject* Item) const override;
};
