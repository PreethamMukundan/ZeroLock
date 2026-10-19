// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "SettingsManagerSubsystem.generated.h"

class USettingsManagerViewModel;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API USettingsManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UPROPERTY(BlueprintReadOnly)
	USettingsManagerViewModel* SettingsManagerVM;
	
	USettingsManagerViewModel* GetSettingsVM();
};
