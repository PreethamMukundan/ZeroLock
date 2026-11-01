//Copyright Preetham Mukundan (C) 2026


#include "UI/BaseComp/MyZL_BasePlayerHealthBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"
#include "UI/MVVM/ZL_VM_Attributes.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UMyZL_BasePlayerHealthBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		return;
	}

	UProgressBar* HealthBar = nullptr;
	WidgetTree->ForEachWidget([this, &HealthBar](UWidget* Widget)
	{
		if (!HealthBar && Widget->IsA<UProgressBar>() && (HealthBarName.IsNone() || Widget->GetFName() == HealthBarName))
		{
			HealthBar = Cast<UProgressBar>(Widget);
		}
	});
	UOverlay* Overlay = HealthBar ? Cast<UOverlay>(HealthBar->GetParent()) : nullptr;
	if (!Overlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] no health bar inside an Overlay: barrier not shown"), *GetName());
		return;
	}

	// Same shape as the health bar, nothing behind the fill, filling from the right end.
	BarrierBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BarrierBar"));
	FProgressBarStyle Style = HealthBar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
	BarrierBar->SetWidgetStyle(Style);
	BarrierBar->SetBarFillType(EProgressBarFillType::RightToLeft);
	BarrierBar->SetFillColorAndOpacity(BarrierColor);
	BarrierBar->SetPercent(0.0f);
	BarrierBar->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(BarrierBar))
	{
		const UOverlaySlot* HealthSlot = Cast<UOverlaySlot>(HealthBar->Slot);
		BarSlot->SetHorizontalAlignment(HealthSlot ? HealthSlot->GetHorizontalAlignment() : HAlign_Fill);
		BarSlot->SetVerticalAlignment(HealthSlot ? HealthSlot->GetVerticalAlignment() : VAlign_Fill);
		if (HealthSlot)
		{
			BarSlot->SetPadding(HealthSlot->GetPadding());
		}
	}

	BarrierText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BarrierText"));
	FSlateFontInfo Font = BarrierText->GetFont();
	Font.Size = BarrierTextSize;
	BarrierText->SetFont(Font);
	BarrierText->SetColorAndOpacity(FSlateColor(FLinearColor(BarrierColor.R, BarrierColor.G, BarrierColor.B, 1.0f)));
	BarrierText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	BarrierText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	BarrierText->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* TextSlot = Overlay->AddChildToOverlay(BarrierText))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Right);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}
}

UZL_VM_Attributes* UMyZL_BasePlayerHealthBar::GetAttributes() const
{
	// The viewmodel the BP's bindings use, so the barrier matches whoever's health this bar shows.
	if (const UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		if (UZL_VM_Attributes* VM = Cast<UZL_VM_Attributes>(View->GetViewModel(TEXT("ZL_VM_Attributes")).GetObject()))
		{
			return VM;
		}
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwningPlayerPawn());
	return Hero ? Hero->GetVM_Attributes() : nullptr;
}

void UMyZL_BasePlayerHealthBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BarrierBar)
	{
		return;
	}
	const UZL_VM_Attributes* VM = GetAttributes();
	const float Barrier = VM ? VM->GetBarrier() : 0.0f;
	const float MaxHealth = VM ? VM->GetMaxHealth() : 0.0f;
	if (Barrier == LastBarrier && MaxHealth == LastMaxHealth)
	{
		return;
	}
	LastBarrier = Barrier;
	LastMaxHealth = MaxHealth;

	const bool bShow = Barrier > 0.0f && MaxHealth > 0.0f;
	const ESlateVisibility BarrierVisibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	BarrierBar->SetVisibility(BarrierVisibility);
	BarrierBar->SetPercent(bShow ? FMath::Clamp(Barrier / MaxHealth, 0.0f, 1.0f) : 0.0f);
	if (BarrierText)
	{
		BarrierText->SetVisibility(BarrierVisibility);
		BarrierText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), FMath::CeilToInt(Barrier))));
	}
}
