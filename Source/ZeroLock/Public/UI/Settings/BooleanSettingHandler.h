// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/SettingHandlerBase.h"
#include "BooleanSettingHandler.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class ZEROLOCK_API UBooleanSettingHandler : public USettingHandlerBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	void OnApplyToggle(bool bNewState);
	
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings Logic")
	bool GetCurrentValue();
	
};
