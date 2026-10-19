// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "SettingsBaseRow.generated.h"

class UCommonTextBlock;
class USettingBaseViewModel;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API USettingsBaseRow : public UCommonButtonBase,public IUserObjectListEntry
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetViewModel(USettingBaseViewModel* InViewModel);
	

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	virtual void SetupComponents(USettingBaseViewModel* InViewModel);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MVVM" ,meta=(BindWidget))
	UCommonTextBlock* NameOfRow;

	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	

	virtual void OnRightClicked();
	virtual void OnLeftClicked();
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;
};
