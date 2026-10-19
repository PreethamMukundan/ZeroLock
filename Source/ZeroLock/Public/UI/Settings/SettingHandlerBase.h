// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SettingHandlerBase.generated.h"

UENUM(BlueprintType)
enum class ESettingInteractionType : uint8
{
	Toggle,
	Slider,
	Discrete, 
	Keybind
};
USTRUCT(BlueprintType)
struct FMasterSubSetting : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Data")
	FName SettingCategory; 
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Data")
	TSoftObjectPtr<UDataTable> SubTable;
	
};
USTRUCT(BlueprintType)
struct FMasterSettingRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Data")
	FName SettingCategory; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Data")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Data")
	ESettingInteractionType InteractionType = ESettingInteractionType::Toggle;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Logic")
	TSubclassOf<USettingHandlerBase> CustomHandlerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Logic", meta = (EditCondition = "InteractionType == ESettingInteractionType::Discrete", EditConditionHides))
	TArray<FText> DiscreteOptions;
};
/**
 * 
 */
UCLASS(Blueprintable)
class ZEROLOCK_API USettingHandlerBase : public UObject
{
	GENERATED_BODY()

};
