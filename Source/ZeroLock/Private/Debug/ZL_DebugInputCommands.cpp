// Copyright Preetham Mukundan (C) 2026

// Console commands that drive the local player like real input, for testing abilities through the editor's console
// (e.g. from automation). Keys go through APlayerController::InputKey, so Enhanced Input sees them like a keyboard.
//   ZL.Debug.Key <KeyName> [HoldSeconds]   e.g. ZL.Debug.Key C 0.1, ZL.Debug.Key W 1.5, ZL.Debug.Key LeftMouseButton
//   ZL.Debug.SetView <Yaw> [Pitch]        sets the local player's control rotation
//   ZL.Debug.Confirm [Cancel]             GAS generic confirm/cancel (target actors)

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"

namespace ZLDebugInput
{
	/** The local player controller of a game world, preferring a networked client (PIE "Play As Client"). */
	static APlayerController* FindLocalPlayerController()
	{
		APlayerController* Fallback = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
			{
				continue;
			}
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (PC && PC->IsLocalController() && PC->GetPawn())
				{
					if (World->GetNetMode() == NM_Client)
					{
						return PC;
					}
					Fallback = Fallback ? Fallback : PC;
				}
			}
		}
		return Fallback;
	}

	static void SendKey(APlayerController* PC, const FKey& Key, EInputEvent Event)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, Event == IE_Released ? 0.0f : 1.0f));
	}

	static FAutoConsoleCommand KeyCommand(
		TEXT("ZL.Debug.Key"),
		TEXT("ZL.Debug.Key <KeyName> [HoldSeconds=0.1]: press a key on the local player controller, release it after HoldSeconds."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			APlayerController* PC = FindLocalPlayerController();
			const FKey Key = Args.Num() > 0 ? FKey(*Args[0]) : FKey();
			if (!PC || !Key.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("ZL.Debug.Key: no local player or invalid key"));
				return;
			}
			const float Hold = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.1f;

			SendKey(PC, Key, IE_Pressed);
			TWeakObjectPtr<APlayerController> WeakPC(PC);
			FTimerHandle Handle;
			PC->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakPC, Key]()
			{
				if (WeakPC.IsValid())
				{
					SendKey(WeakPC.Get(), Key, IE_Released);
				}
			}), FMath::Max(Hold, 0.02f), false);
			UE_LOG(LogTemp, Log, TEXT("ZL.Debug.Key: %s held %.2fs (%s)"), *Key.ToString(), Hold, *PC->GetName());
		}));

	static FAutoConsoleCommand ConfirmCommand(
		TEXT("ZL.Debug.Confirm"),
		TEXT("ZL.Debug.Confirm [Cancel]: GAS generic confirm (or cancel) on the local player's ASC, like the Confirm/Cancel input."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			APlayerController* PC = FindLocalPlayerController();
			UAbilitySystemComponent* ASC = PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
			if (!ASC)
			{
				return;
			}
			const bool bCancel = Args.Num() > 0 && Args[0].Equals(TEXT("Cancel"), ESearchCase::IgnoreCase);
			bCancel ? ASC->LocalInputCancel() : ASC->LocalInputConfirm();
			UE_LOG(LogTemp, Log, TEXT("ZL.Debug.Confirm: %s"), bCancel ? TEXT("cancel") : TEXT("confirm"));
		}));

	static FAutoConsoleCommand SetViewCommand(
		TEXT("ZL.Debug.SetView"),
		TEXT("ZL.Debug.SetView <Yaw> [Pitch=0]: set the local player's control rotation."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			APlayerController* PC = FindLocalPlayerController();
			if (!PC || Args.Num() < 1)
			{
				return;
			}
			const float Pitch = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
			PC->SetControlRotation(FRotator(Pitch, FCString::Atof(*Args[0]), 0.0f));
			UE_LOG(LogTemp, Log, TEXT("ZL.Debug.SetView: %s pawn at %s"), *PC->GetControlRotation().ToString(), *PC->GetPawn()->GetActorLocation().ToString());
		}));
}
