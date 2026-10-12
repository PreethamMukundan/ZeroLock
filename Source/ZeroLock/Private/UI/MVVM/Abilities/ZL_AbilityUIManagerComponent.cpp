//Copyright Preetham Mukundan (C) 2026


#include "UI/MVVM/Abilities/ZL_AbilityUIManagerComponent.h"

#include "GameplayTagContainer.h"
#include "GAS/BaseCharAbilitySystemComponent.h"
#include "GAS/BaseCharAttributeSet.h"
#include "UI/MVVM/Abilities/ZL_VM_AbilitiesContainer.h"
#include "UI/MVVM/Abilities/ZL_VM_AbilityIcon.h"
#include "UI/MVVM/Abilities/ZL_VM_AbilityTimerProgressBar.h"
#include "UI/MVVM/Abilities/ZL_VM_ProgressionStack.h"
#include "ZeroLock/ZeroLockCharacter.h"

UZL_AbilityUIManagerComponent::UZL_AbilityUIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
}

UObject* UZL_AbilityUIManagerComponent::GetAbilitiesViewModel() 
{
	if (!VM_Abilities)
	{
		
		VM_Abilities = NewObject<UZL_VM_AbilitiesContainer>(GetOwner());
		if (VM_Abilities)
		{
			VM_Abilities->InitSlots();
		}
	}
	
	return VM_Abilities;
}

void UZL_AbilityUIManagerComponent::AbilityUpgradeCallBackFromUI(UZL_VM_AbilityIcon* AbilityIconVM, int32 NewLevel)
{
	if (NewLevel > 3) return;
    
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
	if (Hero && AbilityIconVM)
	{
		if (UGameplayAbility* GAToUpgrade = *SlotToAbilityMap.Find(AbilityIconVM))
		{
			Server_UpgradeAbility(GAToUpgrade->GetClass(), NewLevel);
			AbilityIconVM->SetAbilityLevel(NewLevel);
		}
	}
}

void UZL_AbilityUIManagerComponent::Server_UpgradeAbility_Implementation(TSubclassOf<UBaseGameplayAbility> AbilityClass,
	int32 NewLevel)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
	if (!Hero) return;

	UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
	if (!ASC || !AbilityClass) return;

	if (FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromClass(AbilityClass))
	{
		if (AbilitySpec->Level < NewLevel)
		{
			AbilitySpec->Level = NewLevel; 
			ASC->MarkAbilitySpecDirty(*AbilitySpec);
		}
	}
}

bool UZL_AbilityUIManagerComponent::Server_UpgradeAbility_Validate(TSubclassOf<UBaseGameplayAbility> AbilityClass,
	int32 NewLevel)
{
	return true;
}




void UZL_AbilityUIManagerComponent::AbilityUpgradeCallBackFromASC(UGameplayAbility* AbilityLeveledUp, int32 NewLevel)
{
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
	if (Hero)
	{
		if (UZL_VM_AbilityIcon* TargetSlot = *SlotToAbilityMap.FindKey(AbilityLeveledUp))
		{
			TargetSlot->SetAbilityLevel(NewLevel);
		}
	}
}


void UZL_AbilityUIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetAbilitiesViewModel())
	{
		if (VM_Abilities)
		{
			VM_Abilities->InitSlots();
			if (VM_Abilities->GetSlot_Secondary())
			{
				VM_Abilities->GetSlot_Secondary()->OnAbilityLevelChanged.AddDynamic(this,&UZL_AbilityUIManagerComponent::AbilityUpgradeCallBackFromUI);
			}
			if (VM_Abilities->GetSlot_Ability1())
			{
				VM_Abilities->GetSlot_Ability1()->OnAbilityLevelChanged.AddDynamic(this,&UZL_AbilityUIManagerComponent::AbilityUpgradeCallBackFromUI);
			}
			if (VM_Abilities->GetSlot_Ability2())
			{
				VM_Abilities->GetSlot_Ability2()->OnAbilityLevelChanged.AddDynamic(this,&UZL_AbilityUIManagerComponent::AbilityUpgradeCallBackFromUI);
			}
			if (VM_Abilities->GetSlot_Ultimate())
			{
				VM_Abilities->GetSlot_Ultimate()->OnAbilityLevelChanged.AddDynamic(this,&UZL_AbilityUIManagerComponent::AbilityUpgradeCallBackFromUI);
			}
			InitializeTagMap();
		}
	}
	AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
	if (Hero && Hero->IsLocallyControlled())
	{
		UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
		Hero->GetMyAbilitySystemComp()->OnAbilityUpgraded.AddDynamic(this,&ThisClass::AbilityUpgradeCallBackFromASC);
		Hero->GetMyAbilitySystemComp()->OnNewAbilityAdded.AddUniqueDynamic(this, &ThisClass::OnAbilityAdded);
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::OnGEApplied);
		TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;
		ASC->GetAllAbilities(AbilitySpecHandles);
        
		for (FGameplayAbilitySpecHandle Handle : AbilitySpecHandles)
		{
			if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
			{
				OnAbilityAdded(*Spec);
			}
		}
	}
	
	
}

void UZL_AbilityUIManagerComponent::OnStackTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (StackTagToSlotMap.Contains(CallbackTag))
	{
		UZL_VM_AbilityIcon* Slot = StackTagToSlotMap[CallbackTag];
		if (Slot)
		{
			Slot->SetStackNum(NewCount);
		}
	}
}

void UZL_AbilityUIManagerComponent::OnAbilityAdded(FGameplayAbilitySpec& Spec)
{
	
	UBaseGameplayAbility* Ability = Cast<UBaseGameplayAbility>(Spec.Ability);
	if (!Ability) return;

	UZL_VM_AbilityIcon* TargetSlot = nullptr;
	FGameplayTagContainer CombinedTags = Spec.GetDynamicSpecSourceTags();


	for (const FGameplayTag& Tag : CombinedTags)
	{
		if (AbilityTagMap.Contains(Tag))
		{
			TargetSlot = AbilityTagMap[Tag];
			break; 
		}
	}

	if (TargetSlot)
	{
		
		SlotToAbilityMap.Add(TargetSlot, Ability);
		TargetSlot->SetIconTexture(Ability->IconImage);
		TargetSlot->SetAbilityDescription(FText::FromString(Ability->AbilityDescription));
		TargetSlot->SetAbilityLevel1Description(FText::FromString(Ability->AbilityDescription1));
		TargetSlot->SetAbilityLevel2Description(FText::FromString(Ability->AbilityDescription2));
		TargetSlot->SetAbilityLevel3Description(FText::FromString(Ability->AbilityDescription3));
		TargetSlot->SetAbilityName(FText::FromString(Ability->AbilityName));
		TargetSlot->SetbHasCharges(false);
		if (Ability->bIsChargedAbility)
		{
			AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
			if (Hero && Hero->GetAbilitySystemComponent())
			{
				FGameplayAttribute ChargeAttr = GetChargeAttributeForSlot(Ability->Slot);
             
				if (ChargeAttr.IsValid())
				{
					TargetSlot->SetbHasCharges(true);

					float CurrentVal = Hero->GetAbilitySystemComponent()->GetNumericAttribute(ChargeAttr);
					TargetSlot->SetAbilityCharges((int32)CurrentVal);
					
					Hero->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(ChargeAttr).AddUObject(this, &ThisClass::OnChargeAttributeChanged, TargetSlot);
				}
				ChargeAttr = GetMaxChargeAttributeForSlot(Ability->Slot);
				if (ChargeAttr.IsValid())
				{
					float CurrentVal = Hero->GetAbilitySystemComponent()->GetNumericAttribute(ChargeAttr);
					TargetSlot->SetMaxAbilityCharges((int32)CurrentVal);
					
					Hero->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(ChargeAttr).AddUObject(this, &ThisClass::OnMaxChargeAttributeChanged, TargetSlot);
				
				}
			}
		}
		
		const FGameplayTagContainer* CooldownTags = Ability->GetCooldownTags();
		if (CooldownTags)
		{
			for (FGameplayTag Tag : *CooldownTags)
			{
				TagToSlotMap.Add(Tag, TargetSlot);
				Cast<AZeroLockCharacter>(GetOwner())->GetAbilitySystemComponent()
					->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
					.AddUObject(this, &ThisClass::OnCooldownTagChanged);
			}
		}
		AZeroLockCharacter* Hero = Cast<AZeroLockCharacter>(GetOwner());
		if (!Hero) return;
		FGameplayTag StackTag = Ability->StackTag; 
		if (StackTag.IsValid())
		{
			TargetSlot->SetbHasStacks(true);
			StackTagToSlotMap.Add(StackTag, TargetSlot);
		}
	}
}


void UZL_AbilityUIManagerComponent::InitializeTagMap()
{
	if (!VM_Abilities)return;
	AbilityTagMap.Empty();
	AbilityTagMap.Add(ZerolockGameplayTagsForBinding::TAG_INPUT_SECONDRY, VM_Abilities->Slot_Secondary);
	AbilityTagMap.Add(ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_1, VM_Abilities->Slot_Ability1);
	AbilityTagMap.Add(ZerolockGameplayTagsForBinding::TAG_INPUT_ABILITY_2, VM_Abilities->Slot_Ability2);
	AbilityTagMap.Add(ZerolockGameplayTagsForBinding::TAG_INPUT_ULTIMATE,  VM_Abilities->Slot_Ultimate);
}

void UZL_AbilityUIManagerComponent::OnStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount)
{
	if (ActiveHandleToSlotMap.Contains(Handle))
	{
		ActiveHandleToSlotMap[Handle]->SetStackNum(NewCount);
	}
}

void UZL_AbilityUIManagerComponent::OnGEApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& SpecApplied,
	FActiveGameplayEffectHandle ActiveHandle)
{
	FGameplayTagContainer AssetTags;
	SpecApplied.GetAllAssetTags(AssetTags);
	
	for (const FGameplayTag& Tag : AssetTags)
	{
		if (StackTagToSlotMap.Contains(Tag))
		{
			UZL_VM_AbilityIcon* TargetSlot = StackTagToSlotMap[Tag];
			
			ActiveHandleToSlotMap.Add(ActiveHandle, TargetSlot);
			
			ASC->OnGameplayEffectStackChangeDelegate(ActiveHandle)->AddUObject(this, &ThisClass::OnStackChanged);
			
			TargetSlot->SetStackNum(SpecApplied.GetStackCount());
			break;
		}
	}
}


void UZL_AbilityUIManagerComponent::OnCooldownTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		if (!CooldownTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this, &ThisClass::RefreshCooldowns, 0.033f, true);
		}
	}
}

void UZL_AbilityUIManagerComponent::RefreshCooldowns()
{
	auto ASC = Cast<AZeroLockCharacter>(GetOwner())->GetAbilitySystemComponent();
	bool bAnyActive = false;

	for (auto& Element : TagToSlotMap)
	{
		if (ASC->GetTagCount(Element.Key) > 0)
		{
			bAnyActive = true;
			float Remaining = 0.f, Duration = 0.f;
			FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Element.Key));
			TArray<TPair<float, float>> Times = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
			if (Times.Num() > 0)
			{
				Element.Value->UpdateCooldown(Times[0].Key, Times[0].Value);
			}
		}
		else
		{
			Element.Value->UpdateCooldown(0.f, 0.f);
		}
	}

	if (!bAnyActive)
	{
		GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}
}

UZL_VM_ProgressionStack* UZL_AbilityUIManagerComponent::GetAbilityProgressionStack()
{
	if (!VM_ProgressBarStack)
	{
		VM_ProgressBarStack = NewObject<UZL_VM_ProgressionStack>(GetOwner());
	}
	return VM_ProgressBarStack;
}

UZL_VM_AbilityTimerProgressBar* UZL_AbilityUIManagerComponent::AddProgressBarVM(FName Abilityname)
{
	UZL_VM_AbilityTimerProgressBar* VM_ProgressBar  = NewObject<UZL_VM_AbilityTimerProgressBar>(GetOwner());
	if (VM_ProgressBar)
	{
		VM_ProgressBar->SetAbilityName(FText::FromName(Abilityname));
	}
	GetAbilityProgressionStack()->AddProgressMeter(VM_ProgressBar);
	
	return VM_ProgressBar;
}

void UZL_AbilityUIManagerComponent::RemoveProgressBarVM(UZL_VM_AbilityTimerProgressBar* AbilitynameToRemove)
{
	GetAbilityProgressionStack()->RemoveProgressMeter(AbilitynameToRemove);
}

void UZL_AbilityUIManagerComponent::OnChargeAttributeChanged(const FOnAttributeChangeData& Data,
                                                             UZL_VM_AbilityIcon* SlotVM)
{
	SlotVM->SetAbilityCharges((int32)Data.NewValue);
}

void UZL_AbilityUIManagerComponent::OnMaxChargeAttributeChanged(const FOnAttributeChangeData& Data,
	UZL_VM_AbilityIcon* SlotVM)
{
	SlotVM->SetMaxAbilityCharges((int32)Data.NewValue);
}

FGameplayAttribute UZL_AbilityUIManagerComponent::GetChargeAttributeForSlot(EGameplayAbilitySlot Slot) const
{
	switch (Slot)
	{
		case EGameplayAbilitySlot::AbilitySlot1: return UBaseCharAttributeSet::GetAbilityCharges_1Attribute();
		case EGameplayAbilitySlot::AbilitySlot2: return UBaseCharAttributeSet::GetAbilityCharges_2Attribute();
		case EGameplayAbilitySlot::AbilitySlot3: return UBaseCharAttributeSet::GetAbilityCharges_3Attribute();
		case EGameplayAbilitySlot::UltimateSlot: return UBaseCharAttributeSet::GetAbilityCharges_4Attribute();
		default: return FGameplayAttribute();
	}
}

FGameplayAttribute UZL_AbilityUIManagerComponent::GetMaxChargeAttributeForSlot(EGameplayAbilitySlot Slot) const
{
	switch (Slot)
	{
	case EGameplayAbilitySlot::AbilitySlot1: return UBaseCharAttributeSet::GetMaxCharges_1Attribute();
	case EGameplayAbilitySlot::AbilitySlot2: return UBaseCharAttributeSet::GetMaxCharges_2Attribute();
	case EGameplayAbilitySlot::AbilitySlot3: return UBaseCharAttributeSet::GetMaxCharges_3Attribute();
	case EGameplayAbilitySlot::UltimateSlot: return UBaseCharAttributeSet::GetMaxCharges_4Attribute();
	default: return FGameplayAttribute();
	}
}

void UZL_AbilityUIManagerComponent::OnAbilityIconChanged(EGameplayAbilitySlot Slot, UTexture2D* newIcon)
{
	ZLOG("IConChanged");
}




