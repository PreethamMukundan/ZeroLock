//Copyright Preetham Mukundan (C) 2026


#include "UI/MVVM/Abilities/ZL_VM_AbilityIcon.h"

#include "ZeroLock/ZeroLock.h"



void UZL_VM_AbilityIcon::SetAbilityCharges(int32 Charges)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(AbilityCharges, Charges))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetChargePercent);
		RefreshCooldownDisplay();
	}
}

void UZL_VM_AbilityIcon::UpdateRecharge(float Remaining, float Duration)
{
	RechargeRemaining = Remaining;
	RechargeDuration = Duration;
	UE_MVVM_SET_PROPERTY_VALUE(RechargePercent, (Duration > 0.f) ? 1.f - Remaining / Duration : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(bIsRecharging, Remaining > 0.f);
	RefreshCooldownDisplay();
}

void UZL_VM_AbilityIcon::SetMaxAbilityCharges(int32 Charges)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxAbilityCharges, Charges))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetChargePercent);
	}
}

void UZL_VM_AbilityIcon::SetbHasCharges(bool mbHasCharges)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(bHasCharges, mbHasCharges))
	{
		RefreshCooldownDisplay();
	}
}

void UZL_VM_AbilityIcon::SetAbilityDescription(FText mAbilityDescription)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(AbilityDescription,mAbilityDescription))
	{
		
	}
}

void UZL_VM_AbilityIcon::SetAbilityLevel1Description(FText mAbilityLevel1Description)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(AbilityLevel1Description,mAbilityLevel1Description))
	{
		
	}
}

void UZL_VM_AbilityIcon::SetAbilityLevel2Description(FText mAbilityLevel2Description)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(AbilityLevel2Description,mAbilityLevel2Description))
	{
		
	}
}

void UZL_VM_AbilityIcon::SetAbilityLevel3Description(FText mAbilityLevel3Description)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(AbilityLevel3Description,mAbilityLevel3Description))
	{
		
	}
}

void UZL_VM_AbilityIcon::SetMaxCoolDownTime(float mMaxCoolDownTime)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxCoolDownTime,mMaxCoolDownTime))
	{
		
	}
}

void UZL_VM_AbilityIcon::SetAbilityName(FText mAbilityName)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(AbilityName,mAbilityName))
	{
		
	}
}

void UZL_VM_AbilityIcon::SetIconTexture(UTexture2D* NewIcon)
{
	
	if (UE_MVVM_SET_PROPERTY_VALUE(IconTexture, NewIcon))
	{
		UpdateCooldown(0,1);
	}
}

void UZL_VM_AbilityIcon::UpdateCooldown(float Remaining, float Duration)
{
	CooldownRemaining = Remaining;
	CooldownDuration = Duration;
	RefreshCooldownDisplay();
}

void UZL_VM_AbilityIcon::RefreshCooldownDisplay()
{
	// Out of charges: the icon is unusable until the next charge, so show that timer (or the lockout, if longer).
	float Remaining = CooldownRemaining;
	float Duration = CooldownDuration;
	if (bHasCharges && AbilityCharges <= 0 && RechargeRemaining > Remaining)
	{
		Remaining = RechargeRemaining;
		Duration = RechargeDuration;
	}

	float NewPercent = (Duration > 0.f) ? (Remaining / Duration) : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent, 1-NewPercent);
	UE_MVVM_SET_PROPERTY_VALUE(bIsOnCooldown, Remaining > 0.f);

	int32 Seconds = FMath::CeilToInt(Remaining);
	UE_MVVM_SET_PROPERTY_VALUE(CooldownText, Seconds > 0 ? FText::AsNumber(Seconds) : FText::GetEmpty());
}

void UZL_VM_AbilityIcon::SetbHasStacks(bool bstacks)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(bHasStacks, bstacks))
	{
		
	}
}

void UZL_VM_AbilityIcon::SetStackNum(int32 NewStackNum)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(StackNum, NewStackNum))
	{
		
	}
}

void UZL_VM_AbilityIcon::IncrementAbilityLevel()
{
	OnAbilityLevelChanged.Broadcast(this,GetAbilityLevel()+1);
}

void UZL_VM_AbilityIcon::SetAbilityLevel(int32 NewAbilityLevel)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(AbilityLevel, NewAbilityLevel))
	{
		
	}
}
