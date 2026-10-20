// Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_HUD_StaminaBar.h"

#include "CommonTextBlock.h"
#include "UI/HUD/ZL_PipBar.h"
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

void UZL_HUD_StaminaBar::Refresh(float Stamina, float MaxStamina)
{
	const int32 PipCount = FMath::Max(0, FMath::RoundToInt(MaxStamina));
	const ZLPipBar::FStyle Style{PipStyle, FullColor, RechargingColor, PipSize, PipSpacing};
	ZLPipBar::Refresh(WidgetTree, PipBox, Pips, Style, Stamina, PipCount);

	const int32 FullPips = FMath::FloorToInt(Stamina + KINDA_SMALL_NUMBER);

	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FullPips, PipCount)));
	}

	LastStamina = Stamina;
	LastMaxStamina = MaxStamina;
}
