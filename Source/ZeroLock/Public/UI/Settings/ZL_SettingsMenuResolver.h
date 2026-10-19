// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "View/MVVMViewModelContextResolver.h"
#include "ZL_SettingsMenuResolver.generated.h"

/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_SettingsMenuResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()
	
public:
	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;
	
};
