// Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_HUD_StaminaBar.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "UI/MVVM/ZL_VM_Attributes.h"
#include "ZeroLock/ZeroLockCharacter.h"

void UZL_HUD_StaminaBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Looked up every frame so respawns and hero swaps need no rebinding.
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwningPlayerPawn());
	UZL_VM_Attributes* VM = Hero ? Hero->GetVM_Attributes() : nullptr;
	if (!VM)
	{
		return;
	}

	if (VM->GetStamina() != LastStamina || VM->GetMaxStamina() != LastMaxStamina)
	{
		Refresh(VM->GetStamina(), VM->GetMaxStamina());
	}
}

void UZL_HUD_StaminaBar::RebuildPips(int32 Count)
{
	if (!PipBox || !WidgetTree)
	{
		return;
	}

	PipBox->ClearChildren();
	Pips.Reset();

	for (int32 i = 0; i < Count; ++i)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SizeBox->SetWidthOverride(PipSize.X);
		SizeBox->SetHeightOverride(PipSize.Y);

		UProgressBar* Pip = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Pip->SetWidgetStyle(PipStyle);
		SizeBox->AddChild(Pip);

		if (UHorizontalBoxSlot* PipSlot = PipBox->AddChildToHorizontalBox(SizeBox))
		{
			PipSlot->SetPadding(FMargin(i == 0 ? 0.0f : PipSpacing, 0.0f, 0.0f, 0.0f));
			PipSlot->SetVerticalAlignment(VAlign_Center);
		}
		Pips.Add(Pip);
	}
}

void UZL_HUD_StaminaBar::Refresh(float Stamina, float MaxStamina)
{
	const int32 PipCount = FMath::Max(0, FMath::RoundToInt(MaxStamina));
	if (PipCount != Pips.Num())
	{
		RebuildPips(PipCount);
	}

	const int32 FullPips = FMath::FloorToInt(Stamina + KINDA_SMALL_NUMBER);
	for (int32 i = 0; i < Pips.Num(); ++i)
	{
		UProgressBar* Pip = Pips[i];
		if (i < FullPips)
		{
			Pip->SetPercent(1.0f);
			Pip->SetFillColorAndOpacity(FullColor);
		}
		else if (i == FullPips)
		{
			Pip->SetPercent(FMath::Frac(Stamina));
			Pip->SetFillColorAndOpacity(RechargingColor);
		}
		else
		{
			Pip->SetPercent(0.0f);
		}
	}

	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FullPips, PipCount)));
	}

	LastStamina = Stamina;
	LastMaxStamina = MaxStamina;
}
