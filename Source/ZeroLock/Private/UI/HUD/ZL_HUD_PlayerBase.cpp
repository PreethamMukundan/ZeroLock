//Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_HUD_PlayerBase.h"
#include "CommonActivatableWidgetSwitcher.h"
#include "UI/BaseComp/MyZL_BasePlayerHealthBar.h"
#include "UI/HUD/ZL_AbilityChargePhase.h"
#include "UI/HUD/ZL_HUD_CrosshairBase.h"

UZL_HUD_PlayerBase::UZL_HUD_PlayerBase()
{
}

void UZL_HUD_PlayerBase::SetCrosshair()
{
	if (CrosshairSwitcher)
	{
		if (GunCrosshair)
		{
			CrosshairSwitcher->SetActiveWidget(GunCrosshair);
		}
	}
}

void UZL_HUD_PlayerBase::CF_ChargePhaseInit(UZL_VM_ChargePercent* NewChargePercent)
{
	if (NewChargePercent && ChargePhaseClass)
	{
		ZL_AbilityChargePhase = CreateWidget<UZL_AbilityChargePhase>(GetWorld(),ChargePhaseClass);
		
		if (ZL_AbilityChargePhase)
		{
			ZL_AbilityChargePhase->SetViewModel(NewChargePercent);
			CrosshairSwitcher->AddChild(ZL_AbilityChargePhase);
			CrosshairSwitcher->SetActiveWidget(ZL_AbilityChargePhase);
		}
	}
	else
	{
		SetCrosshair();
		if (ZL_AbilityChargePhase)
		{
			ZL_AbilityChargePhase->RemoveFromParent();
			ZL_AbilityChargePhase = nullptr;
		}
		
	}
}

void UZL_HUD_PlayerBase::AddToDelegate()
{
}

void UZL_HUD_PlayerBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetCrosshair();
}

