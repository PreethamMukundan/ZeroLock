//Copyright Preetham Mukundan (C) 2026


#include "UI/HeroSelector/ZL_HeroSelectionScreen.h"

#include "CommonTileView.h"
#include "Components/Image.h"
#include "CharacterSelector/ZL_CharacterSelectionSubsystem.h"
#include "CharacterSelector/ZL_CharacterSelectionVM.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"

#include "UI/ItemShop/ZL_ItemShop_Base.h"
#include "ZeroLock/ZeroLock.h"
#include "Zero_BasePlayerController.h"
#include "ZeroLock/ZeroLockCharacter.h"

#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

UZL_HeroSelectionScreen::UZL_HeroSelectionScreen()
{
}

void UZL_HeroSelectionScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (UZL_CharacterSelectionSubsystem* Subsystem = GetSelectionSubsystem())
	{
		SelectionVM = Subsystem->SelectionVM;
		ZLOG("Set IN Widget");

		

			if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
			{

				if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
				{					

					if (bool bSuccess = WidgetView->SetViewModel(FName("Zl_CharacterSelectionVM"), Subsystem->SelectionVM))
					{
						UE_LOG(LogTemp, Log, TEXT("MVVM: Successfully linked Zl_CharacterSelectionVM!"));
					
						
						if (HeroSelectionList && Subsystem->SelectionVM)
						{
							HeroSelectionList->SetListItems(Subsystem->SelectionVM->GetCharacterList());
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("MVVM: Failed to link Viewmodel. Check the name in the BP!"));
					}
				}
			}
		}
		
	


	if (SelectionVM)
	{
		SelectionVM->OnCharacterChanged.AddUniqueDynamic(this, &UZL_HeroSelectionScreen::HandleCharacterChanged);
		HandleCharacterChanged(SelectionVM->CurrentCharacter);
	}

	if (HeroSelectionList)
	{
		HeroSelectionList->OnItemIsHoveredChanged().AddUObject(this, &UZL_HeroSelectionScreen::HandleOnHoveredChanged);

		HeroSelectionList->OnItemClicked().AddUObject(this, &UZL_HeroSelectionScreen::HandleOnSelectionChanged);
		
	}
}

void UZL_HeroSelectionScreen::HandleOnHoveredChanged(UObject* Item, bool bIsHovered)
{
	if (bIsHovered && SelectionVM)
	{
		if (UZL_Character_Data_Asset* Data = Cast<UZL_Character_Data_Asset>(Item))
		{
			SelectionVM->SetCurrentCharacter(Data);
		}
	}
}

void UZL_HeroSelectionScreen::NativeDestruct()
{
	// The VM outlives this widget (game instance).
	if (SelectionVM)
	{
		SelectionVM->OnCharacterChanged.RemoveDynamic(this, &UZL_HeroSelectionScreen::HandleCharacterChanged);
	}
	Super::NativeDestruct();
}

void UZL_HeroSelectionScreen::HandleCharacterChanged(UZL_Character_Data_Asset* NewCharacter)
{
	const FLinearColor Tint = NewCharacter ? FLinearColor(NewCharacter->IconColor) : NoHeroAbilityBackgroundColor;
	for (const FName& WidgetName : AbilityBackgroundWidgetNames)
	{
		if (UImage* Background = Cast<UImage>(GetWidgetFromName(WidgetName)))
		{
			Background->SetColorAndOpacity(Tint);
		}
	}
}

void UZL_HeroSelectionScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	// The VM lives on the game instance and keeps the last hovered hero. Start every open with no preview,
	// otherwise hovering that same hero again doesn't fire OnCharacterChanged.
	if (SelectionVM)
	{
		SelectionVM->SetCurrentCharacter(nullptr);
	}
}

TOptional<FUIInputConfig> UZL_HeroSelectionScreen::GetDesiredInputConfig() const
{
	if (bSwapHeroOnClick)
	{
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	}
	return Super::GetDesiredInputConfig();
}

UWidget* UZL_HeroSelectionScreen::NativeGetDesiredFocusTarget() const
{
	// Give keyboard focus to the list so key presses reach NativeOnKeyDown (they bubble up from it).
	if (bSwapHeroOnClick && HeroSelectionList)
	{
		return HeroSelectionList;
	}
	return Super::NativeGetDesiredFocusTarget();
}

FReply UZL_HeroSelectionScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bSwapHeroOnClick && CloseKey.IsValid() && InKeyEvent.GetKey() == CloseKey && !InKeyEvent.IsRepeat())
	{
		DeactivateWidget();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UZL_HeroSelectionScreen::HandleOnSelectionChanged(UObject* Item)
{
	if (!bSwapHeroOnClick) return;

	UZL_Character_Data_Asset* Data = Cast<UZL_Character_Data_Asset>(Item);
	if (!Data || !Data->CharacterClass) return;

	if (AZero_BasePlayerController* PC = GetOwningPlayer<AZero_BasePlayerController>())
	{
		PC->RequestHero(Data->CharacterClass);
	}
	DeactivateWidget();
}

FReply UZL_HeroSelectionScreen::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	
	FVector2D LocalMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());


	FVector2D WidgetSize = InGeometry.GetLocalSize();


	FVector2D RelativeToCenter = LocalMousePos - (WidgetSize * 0.5f);

	if (SelectionVM)
	{
		SelectionVM->SetLocalMousePos(RelativeToCenter);
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UZL_HeroSelectionScreen::VM_PopulateList(TArray<UZL_Character_Data_Asset*> DataArray)
{
	
	HeroSelectionList->ClearListItems();
	for (UZL_Character_Data_Asset* Data : DataArray)
	{
		ZLOG_COLOR_TIME("VM_PopulateList",FColor::Red,2.0f);
		HeroSelectionList->AddItem(Data);
	}
}

UZL_CharacterSelectionSubsystem* UZL_HeroSelectionScreen::GetSelectionSubsystem() const
{
	if (GetGameInstance())
	{
		return GetGameInstance()->GetSubsystem<UZL_CharacterSelectionSubsystem>();
	}
	return nullptr;
}
