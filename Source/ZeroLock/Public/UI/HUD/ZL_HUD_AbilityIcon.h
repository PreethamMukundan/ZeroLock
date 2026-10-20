//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Abilities/GameplayAbility.h"
#include "FieldNotificationId.h"
#include "Styling/SlateTypes.h"
#include "ZL_HUD_AbilityIcon.generated.h"

class UZL_VM_AbilityIcon;
class AZeroLockCharacter;
struct FGameplayAbilitySpec;
class UBaseGameplayAbility;
class UCommonTextBlock;
class UImage;
class UHorizontalBox;
class UProgressBar;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_HUD_AbilityIcon : public UCommonButtonBase
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetViewModel(UZL_VM_AbilityIcon* InViewModel);

	UPROPERTY(BlueprintReadOnly, Category = "MVVM")
	UZL_VM_AbilityIcon* VM_AbilityIcon;

	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	UCommonTextBlock* Ability_StackText;

	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	UImage* AbilityIcon;

	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	UCommonTextBlock* AbilityChargesText;
	
	/** Old segmented-material charge bar; unused (collapsed) when ChargePipBox exists. */
	UPROPERTY(BlueprintReadWrite,meta=(BindWidgetOptional))
	UImage* ChargeBar;

	/** Stamina-bar style charge pips are built in here (one per max charge, the recharging one filling). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ChargePipBox;

	UPROPERTY(EditAnywhere, Category = "Charges")
	FProgressBarStyle ChargePipStyle;

	UPROPERTY(EditAnywhere, Category = "Charges")
	FLinearColor ChargeFullColor = FLinearColor(1.0f, 0.85f, 0.25f);

	UPROPERTY(EditAnywhere, Category = "Charges")
	FLinearColor ChargeRechargingColor = FLinearColor(0.45f, 0.45f, 0.45f);

	UPROPERTY(EditAnywhere, Category = "Charges")
	FVector2D ChargePipSize = FVector2D(40.0f, 8.0f);

	UPROPERTY(EditAnywhere, Category = "Charges")
	float ChargePipSpacing = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip")
	TSubclassOf<class UZL_AbilityToolTips> TooltipWidgetClass;

	virtual void NativeOnClicked() override;

	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void AbilityChargeUpdates(int32 chargeChange);

protected:
	/** While the slot's ability has charges: pips in ChargePipBox (charges + recharge progress), or without it the old
	 * ChargeBar material (CurrentStep/NumSteps) and the count text. */
	void RefreshChargeDisplay();
	void OnChargeFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> ChargePips;
};
