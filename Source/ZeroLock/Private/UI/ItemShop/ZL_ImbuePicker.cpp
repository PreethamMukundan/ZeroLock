// Copyright Preetham Mukundan (C) 2026

#include "UI/ItemShop/ZL_ImbuePicker.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GAS/BaseGameplayAbility.h"
#include "Items/Zero_Item_Inventory_Component.h"
#include "UI/HUD/ZL_RoundImage.h"
#include "ZeroLock/ZeroLock.h"

namespace
{
	const EGASAbilityInputID ImbueSlots[] = { EGASAbilityInputID::Secondry_Attack, EGASAbilityInputID::Ability_1, EGASAbilityInputID::Ability_2, EGASAbilityInputID::Ultimate };

	/** Every choice is this wide, so a long ability name can't stretch its icon. */
	constexpr float ChoiceWidth = 140.0f;
	const FLinearColor BoxColor(0.012f, 0.016f, 0.016f, 0.97f);
	const FLinearColor BoxOutlineColor(0.05f, 0.16f, 0.1f, 1.0f);
}

void UZL_ImbuePicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// User widgets default to letting the mouse through, and this one has to catch every click.
	SetVisibility(ESlateVisibility::Visible);

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
}

void UZL_ImbuePicker::Build()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(FSlateRoundedBoxBrush(BoxColor, 10.0f, BoxOutlineColor, 2.0f));
	Box->SetPadding(FMargin(32.0f, 28.0f));
	if (UOverlaySlot* BoxSlot = Root->AddChildToOverlay(Box))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Box->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(NSLOCTEXT("ZeroLock", "ImbueTitle", "Choose an ability to imbue"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Row);

	Choices.SetNum(UE_ARRAY_COUNT(ImbueSlots));
	for (FZL_ImbueChoice& Choice : Choices)
	{
		Choice.Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Choice.Cell))
		{
			CellSlot->SetPadding(FMargin(8.0f, 0.0f));
		}

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(IconSize);
		Size->SetHeightOverride(IconSize);
		if (UVerticalBoxSlot* IconSlot = Choice.Cell->AddChildToVerticalBox(Size))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
		}

		Choice.Icon = WidgetTree->ConstructWidget<UZL_RoundImage>(UZL_RoundImage::StaticClass());
		Choice.Icon->OutlineThickness = 2.0f;
		Choice.Icon->SetDesiredSizeOverride(FVector2D(IconSize));
		Size->AddChild(Choice.Icon);

		USizeBox* NameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NameSize->SetWidthOverride(ChoiceWidth);
		if (UVerticalBoxSlot* NameSlot = Choice.Cell->AddChildToVerticalBox(NameSize))
		{
			NameSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		Choice.Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Choice.Name->SetAutoWrapText(true);
		Choice.Name->SetJustification(ETextJustify::Center);
		NameSize->AddChild(Choice.Name);
	}
}

void UZL_ImbuePicker::Open(UZero_Item_data* Item, UZero_Item_Inventory_Component* Inventory)
{
	PendingItem = Item;
	PendingInventory = Inventory;

	const UAbilitySystemComponent* ASC = Inventory ? Inventory->GetASC() : nullptr;
	for (int32 Index = 0; Index < Choices.Num(); Index++)
	{
		FZL_ImbueChoice& Choice = Choices[Index];
		Choice.InputID = 0;

		const int32 InputID = static_cast<int32>(ImbueSlots[Index]);
		const UBaseGameplayAbility* Ability = nullptr;
		if (ASC)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.InputID == InputID)
				{
					Ability = Cast<UBaseGameplayAbility>(Spec.Ability);
					break;
				}
			}
		}

		// A hero may leave a slot empty.
		Choice.Cell->SetVisibility(Ability ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (Ability)
		{
			Choice.InputID = InputID;
			Choice.Icon->SetBrushFromTexture(Ability->IconImage);
			Choice.Name->SetText(FText::FromString(Ability->AbilityName));
		}
	}
}

FReply UZL_ImbuePicker::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	for (const FZL_ImbueChoice& Choice : Choices)
	{
		if (Choice.InputID != 0 && PendingInventory && Choice.Cell->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			PendingInventory->ServerBuyImbuedItem(PendingItem, Choice.InputID);
			break;
		}
	}

	PendingItem = nullptr;
	PendingInventory = nullptr;
	RemoveFromParent();
	return FReply::Handled();
}
