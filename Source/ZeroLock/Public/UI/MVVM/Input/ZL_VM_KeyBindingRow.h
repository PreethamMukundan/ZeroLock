// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ZL_VM_KeyBindingRow.generated.h"

class UKeybindManagerSubsystem;
class UInputAction;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_VM_KeyBindingRow : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	const UInputAction* TargetAction;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Keybindings")
	UKeybindManagerSubsystem* SubsystemRef;
	
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Keybindings")
	FText ActionName;
	
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Keybindings")
	FName MappingName;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "Keybindings")
	FInputChord CurrentKey;

	/** Player-mappable settings DisplayCategory (Movement / Abilities / Combat); the keybinding screen groups rows by it. */
	UPROPERTY(BlueprintReadOnly, Category = "Keybindings")
	FText DisplayCategory;

	void SetSubsystemRef(UKeybindManagerSubsystem* SubsystemRefX);
	UKeybindManagerSubsystem* GetSubsystemRef()const{return SubsystemRef;};
	
	void SetMappingName(FName MappingNameX);
	FName GetMappingName() const{return MappingName;}
	
	
	void SetActionName(FText InName);
	FText GetActionName() const { return ActionName; }

	void SetCurrentKey(FInputChord InKey);
	FInputChord GetCurrentKey() const { return CurrentKey; }
	
	UFUNCTION(BlueprintCallable, Category = "Keybindings")
	void ApplyNewKey(const FInputChord& SelectedKey);
	
};
