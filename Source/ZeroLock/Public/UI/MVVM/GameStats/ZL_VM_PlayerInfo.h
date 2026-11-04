//Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ZL_VM_PlayerInfo.generated.h"

class UZL_VM_Attributes;
class AZero_BasePlayerState;
/**
 * 
 */
UCLASS()
class ZEROLOCK_API UZL_VM_PlayerInfo : public UMVVMViewModelBase
{
	GENERATED_BODY()


public:
	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "Data")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "Data")
	UTexture2D* Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "Data")
	int32 Kills;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "Data")
	int32 Deaths;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "Data")
	int32 Assists;

	UPROPERTY(BlueprintReadOnly, FieldNotify,Setter,Getter, Category = "Data")
	UZL_VM_Attributes* VM_Attributes;


	void SetVM_Attributes(UZL_VM_Attributes* newAttVM);
	void SetPlayerName(const FText& NewPlayerName);
	void SetIcon(UTexture2D* NewIcon);
	void SetKills(int32 NewKills);
	void SetDeaths(int32 NewDeaths);
	void SetAssists(int32 NewAssists);

	FText GetPlayerName()const{return PlayerName;};
	int32 GetKills()const{return Kills;};
	int32 GetDeaths()const{return Deaths;};
	int32 GetAssists()const{return Assists;};
	UTexture2D* GetIcon()const{return Icon;};
	UZL_VM_Attributes* GetVM_Attributes()const {return VM_Attributes;};

	/** The player this entry shows. Team bar cards read the live pawn (hero, health) and K/D/A from it. */
	void SetOwningPlayerState(AZero_BasePlayerState* InPlayerState);
	AZero_BasePlayerState* GetOwningPlayerState() const;

private:
	TWeakObjectPtr<AZero_BasePlayerState> OwningPlayerState;
};
