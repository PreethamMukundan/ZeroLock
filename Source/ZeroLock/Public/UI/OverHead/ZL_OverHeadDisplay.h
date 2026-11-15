// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ZL_OverHeadDisplay.generated.h"

class AZeroLockCharacter;
class UZL_VM_Attributes;
/**
 * Overhead bar on other heroes (AZeroLockCharacter::InitializeFloatingStatusBar). SetDisplayedHero hands the hero to every
 * UZL_DebuffStackIcons in the tree (they show the stacks the local player put on it). bShowHealthText off collapses the
 * text blocks next to HealthBar (the health number).
 */
UCLASS()
class ZEROLOCK_API UZL_OverHeadDisplay : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetViewModel(UZL_VM_Attributes* InViewModel);

	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void SetDisplayedHero(AZeroLockCharacter* InHero);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MVVM")
	bool bShowHealthText = false;

protected:
	virtual void NativeOnInitialized() override;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MVVM",meta = (BindWidget))
	class UProgressBar* HealthBar;
	
	UPROPERTY(BlueprintReadOnly, Category = "MVVM")
	UZL_VM_Attributes* VM_Attributes;
	
};
