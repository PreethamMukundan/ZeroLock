// Copyright Preetham Mukundan (C) 2026


#include "UI/OverHead/ZL_OverHeadDisplay.h"
#include "UI/MVVM/ZL_VM_Attributes.h"
#include "MVVMSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "UI/HUD/ZL_DebuffStackIcons.h"
#include "Components/ProgressBar.h"
#include "View/MVVMView.h"
#include "ZeroLock/ZeroLock.h"

void UZL_OverHeadDisplay::SetViewModel(UZL_VM_Attributes* InViewModel)
{
	
	VM_Attributes = InViewModel;
	if (!VM_Attributes) return;
	if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
	{
		
		if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
		{					
			if (bool bSuccess = WidgetView->SetViewModel(FName("ZL_VM_Attributes"), VM_Attributes))
			{
				ZLOG_COLOR_TIME("Init In Widget",FColor::Blue,10);
				HealthBar->SetPercent(VM_Attributes->GetHealthPercentage());
	
			}
		}
	}
}

void UZL_OverHeadDisplay::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (bShowHealthText || !HealthBar)
	{
		return;
	}
	if (const UPanelWidget* Parent = HealthBar->GetParent())
	{
		for (UWidget* Child : Parent->GetAllChildren())
		{
			if (Child && Child->IsA<UTextBlock>())
			{
				Child->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UZL_OverHeadDisplay::SetDisplayedHero(AZeroLockCharacter* InHero)
{
	if (!WidgetTree)
	{
		return;
	}
	WidgetTree->ForEachWidget([InHero](UWidget* Widget)
	{
		if (UZL_DebuffStackIcons* Icons = Cast<UZL_DebuffStackIcons>(Widget))
		{
			Icons->SetHero(InHero);
		}
	});
}
