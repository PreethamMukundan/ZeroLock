//Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_HUD_AbilityIcon.h"

#include <string>

#include "CommonTextBlock.h"
#include "MVVMSubsystem.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/HUD/ZL_PipBar.h"
#include "UI/MVVM/Abilities/ZL_VM_AbilityIcon.h"
#include "View/MVVMView.h"
#include "UI/HUD/ZL_AbilityToolTips.h"
#include "ZeroLock/ZeroLock.h"


void UZL_HUD_AbilityIcon::SetViewModel(UZL_VM_AbilityIcon* InViewModel)
{
	AbilityChargesText->SetVisibility(ESlateVisibility::Hidden);
	Ability_StackText->SetVisibility(ESlateVisibility::Hidden);
	if (VM_AbilityIcon)
	{
		VM_AbilityIcon->RemoveAllFieldValueChangedDelegates(this);
	}
	VM_AbilityIcon = InViewModel;
	if (!VM_AbilityIcon) return;

	// Charges arrive after the HUD is built (the ability or the replicated attributes come later), so follow every change.
	const INotifyFieldValueChanged::FFieldValueChangedDelegate OnChargeField =
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::OnChargeFieldChanged);
	VM_AbilityIcon->AddFieldValueChangedDelegate(UZL_VM_AbilityIcon::FFieldNotificationClassDescriptor::AbilityCharges, OnChargeField);
	VM_AbilityIcon->AddFieldValueChangedDelegate(UZL_VM_AbilityIcon::FFieldNotificationClassDescriptor::MaxAbilityCharges, OnChargeField);
	VM_AbilityIcon->AddFieldValueChangedDelegate(UZL_VM_AbilityIcon::FFieldNotificationClassDescriptor::bHasCharges, OnChargeField);
	VM_AbilityIcon->AddFieldValueChangedDelegate(UZL_VM_AbilityIcon::FFieldNotificationClassDescriptor::RechargePercent, OnChargeField);
	RefreshChargeDisplay();
	if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
	{

		if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
		{					

			if (bool bSuccess = WidgetView->SetViewModel(FName("ZL_VM_AbilityIcon"), VM_AbilityIcon))
			{
			
				if (AbilityIcon && VM_AbilityIcon)
				{
					AbilityIcon->SetBrushFromTexture(VM_AbilityIcon->GetIconTexture());
					if (VM_AbilityIcon->GetbHasStacks())
					{
						Ability_StackText->SetVisibility(ESlateVisibility::Visible);
						FString StackCountText = FString::FromInt(VM_AbilityIcon->GetStackNum());
						Ability_StackText->SetText(FText::FromString(StackCountText));
					}
				}
				if (TooltipWidgetClass)
				{

					UZL_AbilityToolTips* TooltipWidget = CreateWidget<UZL_AbilityToolTips>(GetOwningPlayer(),TooltipWidgetClass);

					if (TooltipWidget)
					{
						ZLOG("ToolTipsSetup")
						TooltipWidget->SetViewModel(InViewModel);
						SetToolTip(TooltipWidget);
					}
				}
	
			}
		}
	}
	
}

void UZL_HUD_AbilityIcon::NativeOnClicked()
{
	Super::NativeOnClicked();
	
}

FReply UZL_HUD_AbilityIcon::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (VM_AbilityIcon)
	{
		VM_AbilityIcon->IncrementAbilityLevel();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UZL_HUD_AbilityIcon::AbilityChargeUpdates(int32 chargeChange)
{
	RefreshChargeDisplay();
}

void UZL_HUD_AbilityIcon::OnChargeFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId)
{
	RefreshChargeDisplay();
}

void UZL_HUD_AbilityIcon::RefreshChargeDisplay()
{
	const bool bShowCharges = VM_AbilityIcon && VM_AbilityIcon->GetbHasCharges();

	if (ChargePipBox)
	{
		if (ChargeBar)
		{
			ChargeBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (AbilityChargesText)
		{
			AbilityChargesText->SetVisibility(ESlateVisibility::Hidden);
		}
		ChargePipBox->SetVisibility(bShowCharges ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowCharges)
		{
			const int32 MaxCharges = VM_AbilityIcon->GetMaxAbilityCharges();
			const int32 Charges = FMath::Clamp(VM_AbilityIcon->GetAbilityCharges(), 0, MaxCharges);
			// Whole part = full pips, fraction = the charge being recharged (same as stamina).
			const float Value = Charges < MaxCharges ? Charges + VM_AbilityIcon->RechargePercent : float(Charges);
			const ZLPipBar::FStyle PipBarStyle{ChargePipStyle, ChargeFullColor, ChargeRechargingColor, ChargePipSize, ChargePipSpacing};
			ZLPipBar::Refresh(WidgetTree, ChargePipBox, ChargePips, PipBarStyle, Value, MaxCharges);
		}
		return;
	}

	if (ChargeBar)
	{
		ChargeBar->SetVisibility(bShowCharges ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowCharges)
		{
			if (UMaterialInstanceDynamic* DynMat = ChargeBar->GetDynamicMaterial())
			{
				DynMat->SetScalarParameterValue(FName("CurrentStep"), VM_AbilityIcon->GetAbilityCharges());
				DynMat->SetScalarParameterValue(FName("NumSteps"), VM_AbilityIcon->GetMaxAbilityCharges());
			}
		}
	}
	if (AbilityChargesText)
	{
		AbilityChargesText->SetVisibility(bShowCharges ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (bShowCharges)
		{
			AbilityChargesText->SetText(FText::AsNumber(VM_AbilityIcon->GetAbilityCharges()));
		}
	}
}
