// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZL_NamedCarouselNavBar.generated.h"

class UCommonButtonBase;
class UCommonButtonGroupBase;
class UZL_BTN_SubMenu;
class UCommonWidgetCarousel;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_NamedCarouselNavBar : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable, Category = "Carousel")
	void SetLinkedCarousel(UCommonWidgetCarousel* Carousel);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carousel")
	TSubclassOf<UZL_BTN_SubMenu> TabButtonClass;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UPanelWidget* TabContainer; 

	UPROPERTY()
	UCommonWidgetCarousel* LinkedCarousel;
	
	UPROPERTY()
	UCommonButtonGroupBase* ButtonGroup;

	UFUNCTION()
	void HandlePageChanged(UCommonWidgetCarousel* CommonCarousel, int32 PageIndex);

	UFUNCTION()
	void HandleButtonClicked(UCommonButtonBase* AssociatedButton, int32 ButtonIndex);

	void RebuildButtons();
};
