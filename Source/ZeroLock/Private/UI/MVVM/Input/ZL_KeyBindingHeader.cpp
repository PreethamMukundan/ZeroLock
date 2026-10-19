// Copyright Preetham Mukundan (C) 2026

#include "UI/MVVM/Input/ZL_KeyBindingHeader.h"

#include "CommonTextBlock.h"

void UZL_KeyBindingHeader::SetHeaderText(const FText& InText)
{
	if (HeaderText)
	{
		HeaderText->SetText(InText);
	}
}
