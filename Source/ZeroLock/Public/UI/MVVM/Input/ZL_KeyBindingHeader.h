// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZL_KeyBindingHeader.generated.h"

class UCommonTextBlock;

/**
 * Section title (Movement / Abilities / Combat) placed between keybinding rows by UZL_KeyBindingScreen.
 */
UCLASS()
class ZEROLOCK_API UZL_KeyBindingHeader : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHeaderText(const FText& InText);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HeaderText;
};
