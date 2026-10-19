#include "Input/KeybindManagerSubsystem.h"

#include "InputMappingContext.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "ZeroLock/ZeroLock.h"

void UKeybindManagerSubsystem::InitializeKeybinds(UInputMappingContext* BaseIMC)
{
    if (!BaseIMC) return;
    
    CachedBaseIMC = BaseIMC;

    
    UEnhancedInputLocalPlayerSubsystem* EISubsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!EISubsystem)
    {
        //ZLOG_COLOR_TIME("FAILED: EISubsystem is NULL! (Called too early?)", FColor::Red, 10.0f);
    }
    else if (!EISubsystem->GetUserSettings())
    {
     //   ZLOG_COLOR_TIME("FAILED: UserSettings is NULL! (Check Project Settings)", FColor::Orange, 10.0f);
    }
    else
    {
        EISubsystem->GetUserSettings()->RegisterInputMappingContext(BaseIMC);
    }

    RefreshKeybindData();
}




void UKeybindManagerSubsystem::RefreshKeybindData()
{
    if (!CachedBaseIMC) return;

    UEnhancedInputLocalPlayerSubsystem* EISubsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!EISubsystem || !EISubsystem->GetUserSettings()) return;

    UEnhancedInputUserSettings* UserSettings = EISubsystem->GetUserSettings();
    UEnhancedPlayerMappableKeyProfile* Profile = UserSettings->GetActiveKeyProfile();

    CurrentKeybinds.Empty();

    TSet<const UInputAction*> ProcessedActions;
    const TArray<FEnhancedActionKeyMapping>& Mappings = CachedBaseIMC->GetMappings();

    for (const FEnhancedActionKeyMapping& Mapping : Mappings)
    {

        if (ProcessedActions.Contains(Mapping.Action)) continue;
        if (Mapping.GetMappingName().IsNone()) continue;
        FKeybindData NewData;
        NewData.Action = Mapping.Action;
        NewData.MappingName = Mapping.GetMappingName();
        NewData.DisplayCategory = Mapping.GetDisplayCategory();
      

        if (!Mapping.GetDisplayName().IsEmpty()) {
            NewData.ActionName = Mapping.GetDisplayName();
        } else if (!Mapping.Action->ActionDescription.IsEmpty()) {
            NewData.ActionName = Mapping.Action->ActionDescription; 
        } else {
            NewData.ActionName = FText::FromName(Mapping.Action->GetFName());
        }

        bool bFoundCustomKey = false;


        if (Profile)
        {

            // Profile rows are keyed by the player-mappable mapping name, not the action name.
            if (const FKeyMappingRow* MappingRow = Profile->FindKeyMappingRow(NewData.MappingName))
            {
                for (const FPlayerKeyMapping& PlayerMapping : MappingRow->Mappings)
                {
                    NewData.CurrentKey = PlayerMapping.GetCurrentKey();
                    bFoundCustomKey = true;
                    break; 
                }
            }
        }

        if (!bFoundCustomKey)
        {
            NewData.CurrentKey = Mapping.Key; 
        }

        CurrentKeybinds.Add(NewData);
        ProcessedActions.Add(Mapping.Action);
    }

    OnKeybindsUpdated.Broadcast();
}

void UKeybindManagerSubsystem::RemapKey(FName MappingName, FKey NewKey)
{
  //  if (MappingName.IsNone()) return;

    UEnhancedInputLocalPlayerSubsystem* EISubsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!EISubsystem || !EISubsystem->GetUserSettings()) return;

    UEnhancedInputUserSettings* UserSettings = EISubsystem->GetUserSettings();

    FMapPlayerKeyArgs Args;
    Args.MappingName = MappingName; 
    Args.NewKey = NewKey;
    Args.Slot = EPlayerMappableKeySlot::First;
    
    FGameplayTagContainer NewContainer;
    UserSettings->MapPlayerKey(Args,NewContainer);
    UserSettings->ApplySettings();
    UserSettings->SaveSettings();


    EISubsystem->RequestRebuildControlMappings(FModifyContextOptions(), EInputMappingRebuildType::RebuildWithFlush);
}