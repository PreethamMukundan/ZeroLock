// Copyright Preetham Mukundan (C) 2026


#include "SCombo/ZL_SCombo_ComboBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "SCombo/ZL_SCombo_Combo.h"
#include "SCombo/ZL_SCombo_Ultimate.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UZL_SCombo_ComboBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Full screen root: it must not eat the mouse.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildBar();
	}
}

TSharedRef<SWidget> UZL_SCombo_ComboBar::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildBar();
	}
	return Super::RebuildWidget();
}

void UZL_SCombo_ComboBar::BuildBar()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UCanvasPanelSlot* StackSlot = Root->AddChildToCanvas(Stack))
	{
		StackSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		StackSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		StackSlot->SetPosition(FVector2D(0.0f, -BottomOffset));
		StackSlot->SetSize(BarSize);
	}

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	Bar->SetPercent(0.0f);
	Bar->SetFillColorAndOpacity(BuildingColor);
	if (UOverlaySlot* BarSlot = Stack->AddChildToOverlay(Bar))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Fill);
	}

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 12;
	Label->SetFont(Font);
	if (UOverlaySlot* LabelSlot = Stack->AddChildToOverlay(Label))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UZL_SCombo_ComboBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwningPlayerPawn());
	const int32 Stacks = Hero ? ZLSCombo::GetComboStacks(Hero->GetMyAbilitySystemComp()) : 0;
	if (Stacks == ShownStacks || !Bar || !Label)
	{
		return;
	}
	ShownStacks = Stacks;

	const int32 MaxStacks = FMath::Max(ZLSCombo::GetMaxComboStacks(), 1);
	const UZL_SCombo_Ultimate* Ultimate = Hero ? Cast<UZL_SCombo_Ultimate>(Hero->UltimateAbility.GetDefaultObject()) : nullptr;
	const bool bReady = Ultimate && Stacks >= Ultimate->GetMinComboToCast();

	Bar->SetPercent(static_cast<float>(Stacks) / MaxStacks);
	Bar->SetFillColorAndOpacity(bReady ? ReadyColor : BuildingColor);
	Label->SetText(FText::FromString(FString::Printf(TEXT("COMBO %d / %d"), Stacks, MaxStacks)));
}
