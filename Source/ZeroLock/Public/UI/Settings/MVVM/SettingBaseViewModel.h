// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "SettingBaseViewModel.generated.h"

class USettingHandlerBase;
enum class ESettingInteractionType : uint8;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API USettingBaseViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter)
	ESettingInteractionType InteractionType;
	
	UPROPERTY(BlueprintReadOnly, Getter)
	USettingHandlerBase* BaseHandler;
	
	
	FText GetDisplayName() const{return  DisplayName;};
	ESettingInteractionType GetInteractionType() const{return  InteractionType;};
	USettingHandlerBase* GetBaseHandler() const{return  BaseHandler;};
	
	void SetInteractionType(const ESettingInteractionType XInteractionType);
	void SetDisplayName(const FText &DisplayName);
	
	
	
	
};
