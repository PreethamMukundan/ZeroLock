// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ZL_BTN_SubMenu.generated.h"

class UCommonActivatableWidget;
class UCommonTextBlock;
class UCommonActivatableWidgetSwitcher;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_BTN_SubMenu : public UCommonButtonBase
{
	GENERATED_BODY()
	
	
public:
	virtual void NativeOnInitialized() override;
	void InitializeTab(FName TabName, UCommonActivatableWidget* WidtoSwit, UCommonActivatableWidgetSwitcher* LinkedSwitcher);
	
	virtual void NativeOnClicked() override;

	void SetBTNNAme(FName TabName);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* BTNText;

	int32 TargetIndex;
    
	UPROPERTY()
	UCommonActivatableWidgetSwitcher* TargetSwitcher;
	
	UPROPERTY()
	UCommonActivatableWidget* WidgetToSwitchTO;
};
