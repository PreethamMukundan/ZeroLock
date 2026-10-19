// Copyright Preetham Mukundan (C) 2026


#include "UI/MVVM/Input/ZL_KeyBindingScreen.h"

#include "Components/DynamicEntryBox.h"
#include "UI/MVVM/Input/ZL_KeyBindingHeader.h"
#include "UI/MVVM/Input/ZL_KeyBindingingRow.h"
#include "UI/MVVM/Input/ZL_VM_KeyBindingRow.h"

void UZL_KeyBindingScreen::OnKeybindRowsUpdated(const TArray<UZL_VM_KeyBindingRow*>& NewRows)
{
	if (!KeybindEntryBox ) return;


	KeybindEntryBox->Reset(true);

	// Group rows by category: CategoryOrder first, then any other category in first-seen order.
	TArray<FString> Categories = CategoryOrder;
	for (const UZL_VM_KeyBindingRow* Row : NewRows)
	{
		if (Row)
		{
			Categories.AddUnique(Row->DisplayCategory.ToString());
		}
	}

	for (const FString& Category : Categories)
	{
		bool bHeaderAdded = false;
		for (UZL_VM_KeyBindingRow* Row : NewRows)
		{
			if (!Row || !Row->DisplayCategory.ToString().Equals(Category, ESearchCase::IgnoreCase)) continue;

			if (!bHeaderAdded && HeaderClass && !Category.IsEmpty())
			{
				if (UZL_KeyBindingHeader* Header = KeybindEntryBox->CreateEntry<UZL_KeyBindingHeader>(HeaderClass))
				{
					Header->SetHeaderText(FText::FromString(Category.ToUpper()));
				}
			}
			bHeaderAdded = true;

			if (UZL_KeyBindingingRow* NewEntry = KeybindEntryBox->CreateEntry<UZL_KeyBindingingRow>())
			{
				NewEntry->SetRowViewModel(Row);
			}
		}
	}
}
