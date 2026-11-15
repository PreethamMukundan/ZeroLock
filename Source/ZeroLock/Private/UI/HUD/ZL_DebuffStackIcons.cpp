// Copyright Preetham Mukundan (C) 2026

#include "UI/HUD/ZL_DebuffStackIcons.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/ZL_DebuffStacks.h"
#include "GAS/ZL_GameplayTags.h"
#include "Styling/CoreStyle.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_DebuffStackIcons::UZL_DebuffStackIcons(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibilityInternal(ESlateVisibility::HitTestInvisible);
	FZL_DebuffStackStyle Frost;
	Frost.Letter = FText::FromString(TEXT("I"));
	Styles.Add(ZerolockGameplayTagsForBinding::TAG_DEBUFFSTACK_FROST, Frost);
}

void UZL_DebuffStackIcons::SetHero(AZeroLockCharacter* InHero)
{
	Hero = InHero;
}

const FZL_DebuffStackStyle& UZL_DebuffStackIcons::GetStyle(const FGameplayTag& Tag, FText& OutLetter) const
{
	if (const FZL_DebuffStackStyle* Style = Styles.Find(Tag))
	{
		OutLetter = Style->Letter;
		return *Style;
	}
	FString Name = Tag.GetTagName().ToString();
	int32 Dot = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), Dot))
	{
		Name.RightChopInline(Dot + 1);
	}
	OutLetter = FText::FromString(Name.Left(1).ToUpper());
	return DefaultStyle;
}

void UZL_DebuffStackIcons::GatherIcons(TArray<FIcon>& Out) const
{
	Out.Reset();
	if (IsDesignTime())
	{
		Out.Add({ ZerolockGameplayTagsForBinding::TAG_DEBUFFSTACK_FROST, 2, 0.6f });
		Out.Add({ ZerolockGameplayTagsForBinding::TAG_DEBUFFSTACK, 1, 0.3f });
		return;
	}
	AZeroLockCharacter* Target = Hero.IsValid() ? Hero.Get() : Cast<AZeroLockCharacter>(GetOwningPlayerPawn());
	UBaseCharAbilitySystemComponent* ASC = Target ? Cast<UBaseCharAbilitySystemComponent>(Target->GetAbilitySystemComponent()) : nullptr;
	if (!ASC || Target->bIsDead)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const double ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	const APawn* LocalPawn = GetOwningPlayerPawn();

	for (const UZL_VM_DebuffStack* Entry : ASC->GetDebuffStacksVM()->GetEntries())
	{
		if (!Entry || Entry->GetStackCount() <= 0 || (bOnlyLocalInstigator && (!LocalPawn || Entry->GetInstigator() != LocalPawn)))
		{
			continue;
		}
		const float Remaining = Entry->GetRemainingPercent(ServerTime);
		if (Entry->GetDuration() > 0.f && Remaining <= 0.f)
		{
			continue; // expired here, the removal just hasn't replicated yet
		}
		// One icon per tag: stacks from several instigators (own HUD) add up.
		if (FIcon* Same = Out.FindByPredicate([Entry](const FIcon& I) { return I.Tag == Entry->GetTag(); }))
		{
			Same->Stacks += Entry->GetStackCount();
			Same->Remaining = FMath::Max(Same->Remaining, Remaining);
			continue;
		}
		Out.Add({ Entry->GetTag(), Entry->GetStackCount(), Remaining });
	}
}

namespace
{
	FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
	}

	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(Size, 5)));
	}
}

TSharedRef<SWidget> UZL_DebuffStackIcons::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildRoot();
	}
	if (IsDesignTime())
	{
		Refresh(FVector2D::ZeroVector); // sample icons in the designer
	}
	return Super::RebuildWidget();
}

void UZL_DebuffStackIcons::BuildRoot()
{
	// Overlay root so the row can sit left, centre or right of whatever width it gets.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DebuffRoot"));
	WidgetTree->RootWidget = Root;
	Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DebuffRow"));
	if (UOverlaySlot* RowSlot = Root->AddChildToOverlay(Row))
	{
		RowSlot->SetHorizontalAlignment(IconAlignment);
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}
	Pool.Reset();
	PoolRoots.Reset();
	AppliedSize = -1.f;
}

UZL_DebuffStackIcons::FIconWidgets& UZL_DebuffStackIcons::GetIconWidgets(int32 Index)
{
	while (Pool.Num() <= Index)
	{
		FIconWidgets& W = Pool.AddDefaulted_GetRef();
		W.Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PoolRoots.Add(W.Root);
		if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(W.Root))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		W.Root->AddChild(Column);

		// Circle with the letter, count badge at its bottom right.
		W.CircleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Column->AddChildToVerticalBox(W.CircleBox);
		UOverlay* CircleStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		W.CircleBox->AddChild(CircleStack);
		W.Circle = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UOverlaySlot* CircleSlot = CircleStack->AddChildToOverlay(W.Circle))
		{
			CircleSlot->SetHorizontalAlignment(HAlign_Fill);
			CircleSlot->SetVerticalAlignment(VAlign_Fill);
		}
		W.Letter = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		W.Letter->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* LetterSlot = CircleStack->AddChildToOverlay(W.Letter))
		{
			LetterSlot->SetHorizontalAlignment(HAlign_Center);
			LetterSlot->SetVerticalAlignment(VAlign_Center);
		}
		W.BadgeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (UOverlaySlot* BadgeSlot = CircleStack->AddChildToOverlay(W.BadgeBox))
		{
			BadgeSlot->SetHorizontalAlignment(HAlign_Right);
			BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		UOverlay* BadgeStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		W.BadgeBox->AddChild(BadgeStack);
		W.Badge = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UOverlaySlot* BadgeImageSlot = BadgeStack->AddChildToOverlay(W.Badge))
		{
			BadgeImageSlot->SetHorizontalAlignment(HAlign_Fill);
			BadgeImageSlot->SetVerticalAlignment(VAlign_Fill);
		}
		W.Count = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		W.Count->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* CountSlot = BadgeStack->AddChildToOverlay(W.Count))
		{
			CountSlot->SetHorizontalAlignment(HAlign_Center);
			CountSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Time left.
		W.BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(W.BarBox))
		{
			BarSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		W.Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		W.BarBox->AddChild(W.Bar);

		W.Root->SetVisibility(ESlateVisibility::Collapsed);
		AppliedSize = -1.f; // new widgets need their sizes and styles
	}
	return Pool[Index];
}

void UZL_DebuffStackIcons::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh(MyGeometry.GetLocalSize());
}

void UZL_DebuffStackIcons::Refresh(const FVector2D& Area)
{
	if (!Row || !WidgetTree)
	{
		return;
	}
	TArray<FIcon> Icons;
	GatherIcons(Icons);
	const int32 Count = Icons.Num();

	// Fit: no taller than the space we get, no wider than the row allows.
	const float BarSpace = TimeBarHeight > 0.f ? TimeBarHeight + 2.f : 0.f;
	float Size = IconSize;
	if (Area.Y > 1.f)
	{
		Size = FMath::Min(Size, static_cast<float>(Area.Y) - BarSpace);
	}
	if (Count > 0 && Area.X > 1.f)
	{
		Size = FMath::Min(Size, (static_cast<float>(Area.X) - IconSpacing * (Count - 1)) / Count);
	}
	Size = FMath::Max(FMath::FloorToFloat(Size), 6.f);
	const float Spacing = FMath::RoundToFloat(IconSpacing * Size / FMath::Max(IconSize, 1.f));
	const float BadgeSize = FMath::Max(FMath::RoundToFloat(Size * 0.55f), 9.f);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		GetIconWidgets(Index);
	}
	const bool bResize = !FMath::IsNearlyEqual(Size, AppliedSize);
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		FIconWidgets& W = Pool[Index];
		if (Index >= Count)
		{
			W.Root->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FIcon& Icon = Icons[Index];
		FText Letter;
		const FZL_DebuffStackStyle& Style = GetStyle(Icon.Tag, Letter);
		if (bResize)
		{
			W.Root->SetWidthOverride(Size);
			if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(W.Root->Slot))
			{
				IconSlot->SetPadding(FMargin(Index > 0 ? Spacing : 0.f, 0.f, 0.f, 0.f));
			}
			W.CircleBox->SetWidthOverride(Size);
			W.CircleBox->SetHeightOverride(Size);
			W.BadgeBox->SetWidthOverride(BadgeSize);
			W.BadgeBox->SetHeightOverride(BadgeSize);
			W.Badge->SetBrush(RoundBrush(BadgeColor, BadgeSize * 0.5f, OutlineColor, 1.f));
			W.BarBox->SetHeightOverride(FMath::Max(TimeBarHeight, 1.f));
			FProgressBarStyle BarStyle = W.Bar->GetWidgetStyle();
			BarStyle.BackgroundImage = RoundBrush(TimeBarBackColor, TimeBarHeight * 0.5f, FLinearColor::Transparent, 0.f);
			BarStyle.FillImage = RoundBrush(FLinearColor::White, TimeBarHeight * 0.5f, FLinearColor::Transparent, 0.f);
			BarStyle.MarqueeImage = BarStyle.FillImage;
			W.Bar->SetWidgetStyle(BarStyle);
			W.Bar->SetFillColorAndOpacity(TimeBarColor);
			SetFontSize(W.Letter, FMath::RoundToInt(Size * 0.5f));
			SetFontSize(W.Count, FMath::RoundToInt(BadgeSize * 0.62f));
			W.Letter->SetColorAndOpacity(FSlateColor(TextColor));
			W.Count->SetColorAndOpacity(FSlateColor(TextColor));
		}
		W.Circle->SetBrush(RoundBrush(Style.Color, Size * 0.5f, OutlineColor, 1.f));
		W.Letter->SetText(Letter);
		W.Count->SetText(FText::AsNumber(Icon.Stacks));
		W.BadgeBox->SetVisibility(bShowStackCount ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		W.BarBox->SetVisibility(TimeBarHeight > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		W.Bar->SetPercent(Icon.Remaining);
		W.Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (bResize && Count > 0)
	{
		AppliedSize = Size;
	}
	if (UOverlaySlot* RowSlot = Cast<UOverlaySlot>(Row->Slot))
	{
		RowSlot->SetHorizontalAlignment(IconAlignment);
	}
}
