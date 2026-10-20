//Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_HUDClass.h"

#include "CommonActivatableWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TimerManager.h"
#include "CharacterSelector/ZL_CharacterSelector_Actor.h"
#include "Gamemode/Zero_BaseGameState.h"
#include "Zero_BasePlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UI/ZL_BaseUILayout.h"
#include "UI/ZL_UI_Tags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UZL_BaseUILayout* AZL_HUDClass::ReturnRootLayer()
{
	return RootLayer;
}

void AZL_HUDClass::BeginPlay()
{
	Super::BeginPlay();

	if (!BaseUILayer)
	{
		return;
	}
	if (GetOwningPlayerController())
	{
		if (!GetOwningPlayerController()->IsLocalController()) return;
		RootLayer = Cast<UZL_BaseUILayout>(CreateWidget(GetOwningPlayerController(),BaseUILayer));
		if (RootLayer)
		{
			RootLayer->AddToViewport();
		}

		if (APawn* Pawn = GetOwningPlayerController()->GetPawn())
		{
			HUDPawn = Pawn;
			bHUDHadPawn = true;
		}

		SetupHeroSelectInput();
		GetWorldTimerManager().SetTimer(InitialHeroPickTimer, this, &AZL_HUDClass::UpdateInitialHeroPick, 0.2f, true, 0.f);
	}

	PushHUD();
}

void AZL_HUDClass::UpdateInitialHeroPick()
{
	AZero_BasePlayerController* PC = Cast<AZero_BasePlayerController>(GetOwningPlayerController());
	const AGameStateBase* BaseGS = GetWorld()->GetGameState();
	const AZero_BaseGameState* GS = Cast<AZero_BaseGameState>(BaseGS);
	if (!PC || PC->GetPawn() || (BaseGS && !GS) || (GS && !GS->bPickHeroOnJoin))
	{
		GetWorldTimerManager().ClearTimer(InitialHeroPickTimer);
		return;
	}
	if (!GS) return; // client: game state not replicated yet

	bWaitingForHeroPick = true;
	if (IsHeroSelectOpen() || PC->HasPendingHeroRequest()) return;
	OpenHeroSelect();
}

void AZL_HUDClass::SetupHeroSelectInput()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !HeroSelectWidgetClass || !HeroSelectKey.IsValid()) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem) return;

	// Built at runtime so no input assets are needed; lives on the HUD so it survives hero swaps (the pawn owns gameplay input).
	HeroSelectAction = NewObject<UInputAction>(this, TEXT("IA_HeroSelect"));
	HeroSelectAction->ValueType = EInputActionValueType::Boolean;
	HeroSelectMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_HeroSelect"));
	HeroSelectMappingContext->MapKey(HeroSelectAction, HeroSelectKey);
	Subsystem->AddMappingContext(HeroSelectMappingContext, 1);

	EnableInput(PC);
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(HeroSelectAction, ETriggerEvent::Started, this, &AZL_HUDClass::ToggleHeroSelect);
	}
}

void AZL_HUDClass::ToggleHeroSelect()
{
	if (IsHeroSelectOpen())
	{
		CloseHeroSelect();
	}
	else
	{
		OpenHeroSelect();
	}
}

void AZL_HUDClass::OpenHeroSelect()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !HeroSelectWidgetClass || !IsValid(RootLayer) || IsHeroSelectOpen()) return;

	// Only game modes with bAllowHeroSwap (replicated through the game state), or the join pick while there's no pawn yet.
	const AZero_BaseGameState* GS = GetWorld()->GetGameState<AZero_BaseGameState>();
	const bool bInitialPick = GS && GS->bPickHeroOnJoin && !PC->GetPawn();
	if (!GS || (!GS->bAllowHeroSwap && !bInitialPick)) return;

	if (HeroSelectPreviewClass && !HeroSelectPreview)
	{
		const FTransform SpawnTransform(HeroSelectPreviewLocation);
		HeroSelectPreview = GetWorld()->SpawnActorDeferred<AZL_CharacterSelector_Actor>(HeroSelectPreviewClass, SpawnTransform, this);
		if (HeroSelectPreview)
		{
			// Cosmetic, local player only.
			HeroSelectPreview->SetReplicates(false);
			HeroSelectPreview->FinishSpawning(SpawnTransform);
		}
	}

	const FUITag MenuLayer = FUITag::TryConvert(ZerolockUIGameplayTags::TAG_UI_LAYER_MENU);
	RootLayer->PushWidgetToLayer(MenuLayer, HeroSelectWidgetClass);

	UCommonActivatableWidget* Widget = RootLayer->GetActivatedWidgetInLayer(MenuLayer);
	if (!Widget || !Widget->IsA(HeroSelectWidgetClass))
	{
		HandleHeroSelectDeactivated();
		return;
	}

	HeroSelectWidget = Widget;
	Widget->OnDeactivated().AddUObject(this, &AZL_HUDClass::HandleHeroSelectDeactivated);
	PC->SetShowMouseCursor(true);
}

void AZL_HUDClass::CloseHeroSelect()
{
	if (UCommonActivatableWidget* Widget = HeroSelectWidget.Get())
	{
		// Fires HandleHeroSelectDeactivated, which does the cleanup.
		if (Widget->IsActivated())
		{
			Widget->DeactivateWidget();
			return;
		}
	}
	HandleHeroSelectDeactivated();
}

void AZL_HUDClass::HandleHeroSelectDeactivated()
{
	if (UCommonActivatableWidget* Widget = HeroSelectWidget.Get())
	{
		Widget->OnDeactivated().RemoveAll(this);
	}
	HeroSelectWidget.Reset();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->SetShowMouseCursor(false);
	}

	if (HeroSelectPreview)
	{
		HeroSelectPreview->Destroy();
		HeroSelectPreview = nullptr;
	}
}

void AZL_HUDClass::HandleHeroReady(APawn* Hero)
{
	if (!Hero || Hero == HUDPawn.Get()) return;

	// Old pawn is usually destroyed by now (swap), so remember that we had one rather than checking the pointer.
	const bool bHadPawn = bHUDHadPawn;
	HUDPawn = Hero;
	bHUDHadPawn = true;

	// First pawn: the HUD pushed in BeginPlay is already for it, unless it was pushed pawnless while picking a hero.
	if (bHadPawn || bWaitingForHeroPick)
	{
		bWaitingForHeroPick = false;
		RebuildGameHUD(0);
	}
}

void AZL_HUDClass::RebuildGameHUD(int32 Attempt)
{
	if (!IsValid(RootLayer) || !RootLayer->GameStack || !PlayerInGameHUD) return;

	// HUD resolvers read GetOwningPlayerPawn(). The hero can be ready before the controller points at it
	// (host: PossessedBy runs before SetPawn; client: pawn's PlayerState can replicate before the controller's Pawn).
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !HUDPawn.IsValid()) return;
	if (PC->GetPawn() != HUDPawn.Get())
	{
		if (Attempt < 300)
		{
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &AZL_HUDClass::RebuildGameHUD, Attempt + 1));
		}
		return;
	}

	UCommonActivatableWidgetStack* Stack = RootLayer->GameStack;
	UCommonActivatableWidget* OldHUD = Stack->GetActiveWidget();

	// A fresh instance, not the stack's pool: a pooled one keeps its old pawn bindings (NativeOnInitialized/MVVM resolvers only run once),
	// and ClearWidgets + AddWidget of the same class can hand back the widget that is still being removed, which hides the HUD.
	UCommonActivatableWidget* NewHUD = CreateWidget<UCommonActivatableWidget>(GetOwningPlayerController(), PlayerInGameHUD);
	if (!NewHUD) return;

	Stack->AddWidgetInstance(*NewHUD);

	if (OldHUD && OldHUD != NewHUD)
	{
		RemoveOldHUD(OldHUD, 0);
	}
}

void AZL_HUDClass::RemoveOldHUD(TWeakObjectPtr<UCommonActivatableWidget> OldHUD, int32 Attempt)
{
	if (!OldHUD.IsValid() || !IsValid(RootLayer) || !RootLayer->GameStack) return;

	// Wait until the stack has switched to the new HUD, otherwise removing the active one deactivates it mid-transition.
	if (RootLayer->GameStack->GetActiveWidget() == OldHUD.Get() && Attempt < 60)
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &AZL_HUDClass::RemoveOldHUD, OldHUD, Attempt + 1));
		return;
	}
	RootLayer->GameStack->RemoveWidget(*OldHUD.Get());
}

void AZL_HUDClass::PushHUD()
{
	if (IsValid(RootLayer) && PlayerInGameHUD)
	{
		RootLayer->PushWidgetToLayer(FUITag::TryConvert(ZerolockUIGameplayTags::TAG_UI_LAYER_GAME), PlayerInGameHUD);
	}
}
