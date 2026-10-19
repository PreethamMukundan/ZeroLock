// Copyright Preetham Mukundan (C) 2026


#include "UI/Settings/ZL_SettingsMenuResolver.h"

#include "Blueprint/UserWidget.h"
#include "UI/Settings/MVVM/SettingsManagerSubsystem.h"
#include "UI/Settings/MVVM/SettingsManagerViewModel.h"

UObject* UZL_SettingsMenuResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget,
                                                  const UMVVMView* View) const
{
	if (!UserWidget)
	{
		return nullptr;
	}
	if (ULocalPlayer* LocalPlayer = UserWidget->GetOwningLocalPlayer())
	{
		if (USettingsManagerSubsystem* SettingsSubsystem = LocalPlayer->GetSubsystem<USettingsManagerSubsystem>())
		{
			return SettingsSubsystem->GetSettingsVM();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("SettingsManagerResolver failed to find the USettingsManagerSubsystem."));
	return nullptr;
}
