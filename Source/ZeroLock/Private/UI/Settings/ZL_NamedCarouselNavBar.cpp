// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/ZL_NamedCarouselNavBar.h"

#include "CommonWidgetCarousel.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/Settings/ZL_SettingsSubMenu.h"
#include "UI/Settings/ZL_BTN_SubMenu.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/PanelWidget.h"
#include "ZeroLock/ZeroLock.h"


void UZL_NamedCarouselNavBar::NativeConstruct()
{
    Super::NativeConstruct();
    ButtonGroup = NewObject<UCommonButtonGroupBase>(this);
    ButtonGroup->SetSelectionRequired(true);
    
    ButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UZL_NamedCarouselNavBar::HandleButtonClicked);
}

void UZL_NamedCarouselNavBar::SetLinkedCarousel(UCommonWidgetCarousel* Carousel)
{
    if (LinkedCarousel)
    {
        LinkedCarousel->OnCurrentPageIndexChanged.RemoveAll(this);
    }

    LinkedCarousel = Carousel;

    if (LinkedCarousel)
    {
        LinkedCarousel->OnCurrentPageIndexChanged.AddDynamic(this, &UZL_NamedCarouselNavBar::HandlePageChanged);
    }

    RebuildButtons();
}

void UZL_NamedCarouselNavBar::RebuildButtons()
{
    if (!LinkedCarousel || !TabContainer || !TabButtonClass) return;

    ButtonGroup->RemoveAll();
    TabContainer->ClearChildren();

    int32 NumPages = LinkedCarousel->GetChildrenCount();
    for (int32 i = 0; i < NumPages; i++)
    {
        UWidget* PageWidget = LinkedCarousel->GetWidgetAtIndex(i);
        
        FName TabName = FName("Unknown");
        if (UZL_SettingsSubMenu* SubMenu = Cast<UZL_SettingsSubMenu>(PageWidget))
        {
            TabName = SubMenu->SubMenuName;
        }

        UZL_BTN_SubMenu* NewBtn = CreateWidget<UZL_BTN_SubMenu>(this, TabButtonClass);
        if (NewBtn)
        {
            NewBtn->SetBTNNAme(TabName); 

            ButtonGroup->AddWidget(NewBtn);

           UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(TabContainer->AddChild(NewBtn));
            if (BoxSlot)
            {
                BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                BoxSlot->SetHorizontalAlignment(HAlign_Fill);
                BoxSlot->SetPadding(FMargin(5.0f, 0.0f, 5.0f, 0.0f));
            }
            
        }
    }
    if (NumPages > 0)
    {
        ButtonGroup->SelectButtonAtIndex(LinkedCarousel->GetActiveWidgetIndex());
        ButtonGroup->OnButtonBaseClicked.AddDynamic(this, &UZL_NamedCarouselNavBar::HandleButtonClicked);
    }
}

void UZL_NamedCarouselNavBar::HandlePageChanged(UCommonWidgetCarousel* CommonCarousel, int32 PageIndex)
{
    if (ButtonGroup && ButtonGroup->GetSelectedButtonIndex() != PageIndex)
    {
        ButtonGroup->SelectButtonAtIndex(PageIndex);
    }
}

void UZL_NamedCarouselNavBar::HandleButtonClicked(UCommonButtonBase* AssociatedButton, int32 ButtonIndex)
{
    if (LinkedCarousel && LinkedCarousel->GetActiveWidgetIndex() != ButtonIndex)
    {
        LinkedCarousel->EndAutoScrolling();
        LinkedCarousel->SetActiveWidgetIndex(ButtonIndex);
    }
}