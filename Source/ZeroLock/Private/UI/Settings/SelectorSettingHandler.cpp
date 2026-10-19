// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/SelectorSettingHandler.h"

TArray<FText> USelectorSettingHandler::GetSelectedNames()
{
	TArray<FText> selectedNames;
	for (FString s : AllSelectionNames)
	{
		selectedNames.Add(FText::FromString(s));
	}
	return selectedNames;
}
