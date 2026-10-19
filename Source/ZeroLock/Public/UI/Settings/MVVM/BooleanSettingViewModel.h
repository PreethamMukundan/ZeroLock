// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/MVVM/SettingBaseViewModel.h"
#include "BooleanSettingViewModel.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UBooleanSettingViewModel : public USettingBaseViewModel
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter)
	bool bIsToggled;

	UFUNCTION(BlueprintCallable)
	void ApplyToggle(bool bNewState);
	
	bool GetbIsToggled()const { return bIsToggled; };
	void SetbIsToggled(const bool bNewState);
};
