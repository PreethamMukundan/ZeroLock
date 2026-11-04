// Copyright Preetham Mukundan (C) 2026

#include "Cheats/ZL_CheatMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Cheats/ZL_CheatComponent.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace ZLCheatMenuStyle
{
	static const FLinearColor Panel(0.01f, 0.012f, 0.01f, 0.92f);
	static const FLinearColor Button(0.05f, 0.16f, 0.08f, 1.0f);
	static const FLinearColor Header(0.45f, 0.95f, 0.55f, 1.0f);
	static const FLinearColor Muted(0.7f, 0.7f, 0.7f, 1.0f);
}

void UZL_CheatButton::SetOnPressed(TFunction<void()> InOnPressed)
{
	OnPressed = MoveTemp(InOnPressed);
	OnClicked.AddUniqueDynamic(this, &UZL_CheatButton::HandleClicked);
}

void UZL_CheatButton::HandleClicked()
{
	if (OnPressed)
	{
		OnPressed();
	}
}

UZL_CheatMenu::UZL_CheatMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true);
}

void UZL_CheatMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildMenu();
	}
}

UZL_CheatComponent* UZL_CheatMenu::GetCheats() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->FindComponentByClass<UZL_CheatComponent>() : nullptr;
}

UTextBlock* UZL_CheatMenu::MakeText(const FString& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

void UZL_CheatMenu::AddHeader(const FString& Text)
{
	if (UVerticalBoxSlot* NewSlot = Content->AddChildToVerticalBox(MakeText(Text, 15, ZLCheatMenuStyle::Header)))
	{
		NewSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f));
	}
}

UPanelWidget* UZL_CheatMenu::AddRow()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* NewSlot = Content->AddChildToVerticalBox(Row))
	{
		NewSlot->SetPadding(FMargin(0.f, 2.f));
	}
	return Row;
}

void UZL_CheatMenu::AddPage()
{
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Pages->AddChild(Scroll);
	Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Content);
}

UZL_CheatButton* UZL_CheatMenu::AddButton(UPanelWidget* Parent, const FString& Label, TFunction<void()> OnPressed)
{
	UZL_CheatButton* Button = WidgetTree->ConstructWidget<UZL_CheatButton>(UZL_CheatButton::StaticClass());
	Button->SetBackgroundColor(ZLCheatMenuStyle::Button);
	Button->SetOnPressed(MoveTemp(OnPressed));
	Button->SetContent(MakeText(Label, 12));

	if (UHorizontalBox* Row = Cast<UHorizontalBox>(Parent))
	{
		if (UHorizontalBoxSlot* NewSlot = Row->AddChildToHorizontalBox(Button))
		{
			NewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NewSlot->SetPadding(FMargin(2.f, 0.f));
		}
	}
	else if (Parent)
	{
		Parent->AddChild(Button);
	}
	if (!FirstButton)
	{
		FirstButton = Button;
	}
	return Button;
}

void UZL_CheatMenu::AddLiveLabel(UTextBlock* Text, TFunction<FString()> GetText, TFunction<bool()> IsOn)
{
	LiveLabels.Add({ Text, MoveTemp(GetText), MoveTemp(IsOn) });
}

void UZL_CheatMenu::AddToggle(UPanelWidget* Parent, const FString& Label, TFunction<bool()> IsOn, TFunction<void()> OnPressed)
{
	UZL_CheatButton* Button = AddButton(Parent, Label, MoveTemp(OnPressed));
	TFunction<bool()> IsOnCopy = IsOn;
	AddLiveLabel(Cast<UTextBlock>(Button->GetContent()), [Label, IsOnCopy]()
	{
		return FString::Printf(TEXT("%s: %s"), *Label, IsOnCopy && IsOnCopy() ? TEXT("ON") : TEXT("OFF"));
	}, MoveTemp(IsOn));
}

void UZL_CheatMenu::AddCVarToggle(UPanelWidget* Parent, const FString& Label, const TCHAR* CVarName)
{
	const FString Name = CVarName;
	AddToggle(Parent, Label, [Name]()
	{
		const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
		return CVar && CVar->GetInt() != 0;
	}, [Name]()
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name))
		{
			CVar->Set(CVar->GetInt() != 0 ? TEXT("0") : TEXT("1"), ECVF_SetByConsole);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Cheats] console variable %s not found"), *Name);
		}
	});
}

TSubclassOf<AZeroLockCharacter> UZL_CheatMenu::GetSelectedHeroClass() const
{
	return Heroes.IsValidIndex(SelectedHero - 1) ? Heroes[SelectedHero - 1]->CharacterClass : nullptr;
}

FString UZL_CheatMenu::GetSelectedHeroName() const
{
	return Heroes.IsValidIndex(SelectedHero - 1) ? Heroes[SelectedHero - 1]->CharacterName : FString(TEXT("My Hero"));
}

void UZL_CheatMenu::BuildMenu()
{
	TWeakObjectPtr<UZL_CheatMenu> WeakThis(this);

	// Panel on the left, vertically centered; the rest of the screen stays visible.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(ZLCheatMenuStyle::Panel);
	Panel->SetPadding(FMargin(16.f));
	if (UOverlaySlot* NewSlot = Root->AddChildToOverlay(Panel))
	{
		NewSlot->SetHorizontalAlignment(HAlign_Left);
		NewSlot->SetVerticalAlignment(VAlign_Center);
		NewSlot->SetPadding(FMargin(40.f, 40.f));
	}

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(600.f);
	Size->SetHeightOverride(820.f);
	Panel->SetContent(Size);

	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Size->SetContent(Outer);
	Content = Outer;

	Content->AddChildToVerticalBox(MakeText(TEXT("CHEATS"), 20));
	Content->AddChildToVerticalBox(MakeText(FString::Printf(TEXT("%s / Esc to close. Dev builds only."), *ToggleKey.GetDisplayName().ToString()), 10, ZLCheatMenuStyle::Muted));

	// Tabs.
	UPanelWidget* TabRow = AddRow();
	if (UVerticalBoxSlot* TabSlot = Cast<UVerticalBoxSlot>(TabRow->Slot))
	{
		TabSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 6.f));
	}
	Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	const TCHAR* TabNames[] = { TEXT("Cheats"), TEXT("Debug") };
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(TabNames)); ++Index)
	{
		const FString TabName = TabNames[Index];
		UZL_CheatButton* Tab = AddButton(TabRow, TabName, [WeakThis, Index]()
		{
			if (WeakThis.IsValid() && WeakThis->Pages)
			{
				WeakThis->Pages->SetActiveWidgetIndex(Index);
			}
		});
		AddLiveLabel(Cast<UTextBlock>(Tab->GetContent()), [TabName]() { return TabName; }, [WeakThis, Index]()
		{
			return WeakThis.IsValid() && WeakThis->Pages && WeakThis->Pages->GetActiveWidgetIndex() == Index;
		});
	}

	if (UVerticalBoxSlot* PagesSlot = Outer->AddChildToVerticalBox(Pages))
	{
		PagesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	BuildCheatsPage();
	BuildDebugPage();

	// Close sits under the pages, on every tab.
	Content = Outer;
	UPanelWidget* Row = AddRow();
	if (UVerticalBoxSlot* CloseSlot = Cast<UVerticalBoxSlot>(Row->Slot))
	{
		CloseSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}
	AddButton(Row, TEXT("Close"), [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->CloseMenu();
		}
	});
}

void UZL_CheatMenu::BuildCheatsPage()
{
	AddPage();
	TWeakObjectPtr<UZL_CheatMenu> WeakThis(this);
	// Each button looks the component up when pressed, so it works for whatever pawn the player has by then.
	auto WithCheats = [WeakThis](TFunction<void(UZL_CheatComponent&)> Action) -> TFunction<void()>
	{
		return [WeakThis, Action]()
		{
			if (UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr)
			{
				Action(*Cheats);
			}
		};
	};
	auto IsOn = [WeakThis](EZL_CheatToggle Cheat) -> TFunction<bool()>
	{
		return [WeakThis, Cheat]()
		{
			const UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr;
			return Cheats && Cheats->IsCheatOn(Cheat);
		};
	};
	auto Toggle = [&WithCheats](EZL_CheatToggle Cheat)
	{
		return WithCheats([Cheat](UZL_CheatComponent& Cheats) { Cheats.ToggleCheat(Cheat); });
	};

	AddHeader(TEXT("Player"));
	UPanelWidget* Row = AddRow();
	AddToggle(Row, TEXT("God Mode"), IsOn(EZL_CheatToggle::GodMode), Toggle(EZL_CheatToggle::GodMode));
	AddToggle(Row, TEXT("No Clip"), IsOn(EZL_CheatToggle::NoClip), Toggle(EZL_CheatToggle::NoClip));
	Row = AddRow();
	AddToggle(Row, TEXT("No Cooldowns"), IsOn(EZL_CheatToggle::NoCooldowns), Toggle(EZL_CheatToggle::NoCooldowns));
	AddToggle(Row, TEXT("Infinite Ammo"), IsOn(EZL_CheatToggle::InfiniteAmmo), Toggle(EZL_CheatToggle::InfiniteAmmo));
	Row = AddRow();
	AddButton(Row, TEXT("Refill HP / Stamina / Ammo"), WithCheats([](UZL_CheatComponent& Cheats) { Cheats.ServerRefill(); }));
	AddButton(Row, TEXT("+10000 Souls"), WithCheats([](UZL_CheatComponent& Cheats) { Cheats.ServerAddSouls(10000); }));
	Row = AddRow();
	AddButton(Row, TEXT("Max Ability Levels"), WithCheats([](UZL_CheatComponent& Cheats) { Cheats.ServerMaxAbilityLevels(); }));
	AddButton(Row, TEXT("Teleport to Crosshair"), WithCheats([WeakThis](UZL_CheatComponent& Cheats)
	{
		// Close first so the crosshair is where the player was looking.
		if (WeakThis.IsValid())
		{
			WeakThis->CloseMenu();
		}
		Cheats.TeleportToCrosshair();
	}));
	Row = AddRow();
	AddButton(Row, TEXT("Kill Self"), WithCheats([](UZL_CheatComponent& Cheats) { Cheats.ServerKillSelf(); }));

	// One hero at a time: pick it, then spawn it as an enemy / ally dummy or play as it.
	AddHeader(TEXT("Spawn Hero"));
	for (UZL_Character_Data_Asset* Hero : UZL_CheatComponent::LoadHeroList(this))
	{
		Heroes.Add(Hero);
	}
	auto StepHero = [WeakThis](int32 Step)
	{
		return [WeakThis, Step]()
		{
			if (WeakThis.IsValid())
			{
				const int32 Count = WeakThis->Heroes.Num() + 1;
				WeakThis->SelectedHero = (WeakThis->SelectedHero + Step + Count) % Count;
			}
		};
	};
	UHorizontalBox* SelectorRow = Cast<UHorizontalBox>(AddRow());
	AddButton(SelectorRow, TEXT("  <  "), StepHero(-1));
	Cast<UHorizontalBoxSlot>(SelectorRow->GetSlots().Last())->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	UTextBlock* HeroName = MakeText(TEXT(""), 14);
	HeroName->SetJustification(ETextJustify::Center);
	if (UHorizontalBoxSlot* NameSlot = SelectorRow->AddChildToHorizontalBox(HeroName))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}
	AddLiveLabel(HeroName, [WeakThis]()
	{
		return WeakThis.IsValid() ? FString::Printf(TEXT("%s  (%d/%d)"), *WeakThis->GetSelectedHeroName(), WeakThis->SelectedHero + 1, WeakThis->Heroes.Num() + 1) : FString();
	});
	AddButton(SelectorRow, TEXT("  >  "), StepHero(1));
	Cast<UHorizontalBoxSlot>(SelectorRow->GetSlots().Last())->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	Row = AddRow();
	AddButton(Row, TEXT("Spawn as Enemy"), WithCheats([WeakThis](UZL_CheatComponent& Cheats)
	{
		Cheats.ServerSpawnDummy(WeakThis.IsValid() ? WeakThis->GetSelectedHeroClass() : TSubclassOf<AZeroLockCharacter>(), true, Cheats.bDummyImmortal, Cheats.bDummyStrafe);
	}));
	AddButton(Row, TEXT("Spawn as Ally"), WithCheats([WeakThis](UZL_CheatComponent& Cheats)
	{
		Cheats.ServerSpawnDummy(WeakThis.IsValid() ? WeakThis->GetSelectedHeroClass() : TSubclassOf<AZeroLockCharacter>(), false, Cheats.bDummyImmortal, Cheats.bDummyStrafe);
	}));
	AddButton(Row, TEXT("Play As"), WithCheats([WeakThis](UZL_CheatComponent& Cheats)
	{
		if (const TSubclassOf<AZeroLockCharacter> HeroClass = WeakThis.IsValid() ? WeakThis->GetSelectedHeroClass() : TSubclassOf<AZeroLockCharacter>())
		{
			Cheats.ServerPlayAsHero(HeroClass);
		}
	}));
	Row = AddRow();
	AddToggle(Row, TEXT("Immortal"), [WeakThis]()
	{
		const UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr;
		return Cheats && Cheats->bDummyImmortal;
	}, WithCheats([](UZL_CheatComponent& Cheats) { Cheats.bDummyImmortal = !Cheats.bDummyImmortal; }));
	AddToggle(Row, TEXT("Strafe"), [WeakThis]()
	{
		const UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr;
		return Cheats && Cheats->bDummyStrafe;
	}, WithCheats([](UZL_CheatComponent& Cheats) { Cheats.bDummyStrafe = !Cheats.bDummyStrafe; }));
	Row = AddRow();
	AddButton(Row, TEXT("Remove All Dummies"), WithCheats([](UZL_CheatComponent& Cheats) { Cheats.ServerRemoveDummies(); }));
}

void UZL_CheatMenu::BuildDebugPage()
{
	AddPage();
	TWeakObjectPtr<UZL_CheatMenu> WeakThis(this);
	auto Console = [WeakThis](const FString& Command)
	{
		return [WeakThis, Command]()
		{
			if (APlayerController* PC = WeakThis.IsValid() ? WeakThis->GetOwningPlayer() : nullptr)
			{
				PC->ConsoleCommand(Command);
			}
		};
	};

	AddHeader(TEXT("Live"));
	UTextBlock* Readout = MakeText(TEXT(""), 11, ZLCheatMenuStyle::Muted);
	Readout->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(Readout);
	AddLiveLabel(Readout, [WeakThis]()
	{
		const UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr;
		return Cheats ? Cheats->BuildDebugReadout() : FString();
	});
	UPanelWidget* Row = AddRow();
	AddToggle(Row, TEXT("On-screen Readout"), [WeakThis]()
	{
		const UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr;
		return Cheats && Cheats->bShowDebugOverlay;
	}, [WeakThis]()
	{
		if (UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr)
		{
			Cheats->bShowDebugOverlay = !Cheats->bShowDebugOverlay;
		}
	});

	// Engine Mover / NetworkPrediction debug drawing (this machine).
	AddHeader(TEXT("Mover Debug Draw"));
	Row = AddRow();
	AddCVarToggle(Row, TEXT("Corrections"), TEXT("mover.debug.ShowCorrections"));
	AddCVarToggle(Row, TEXT("Trail"), TEXT("mover.debug.ShowTrail"));
	Row = AddRow();
	AddCVarToggle(Row, TEXT("Trajectory"), TEXT("mover.debug.ShowTrajectory"));
	AddCVarToggle(Row, TEXT("Teleport Diffs"), TEXT("mover.debug.ShowTeleportDiffs"));
	Row = AddRow();
	AddCVarToggle(Row, TEXT("Input Arrows"), TEXT("mover.debug.ShowInputArrows"));
	AddCVarToggle(Row, TEXT("State Arrows"), TEXT("mover.debug.ShowStateArrows"));
	Row = AddRow();
	AddCVarToggle(Row, TEXT("Server Primitives"), TEXT("np.Debug.DrawServerPrimitives"));
	AddCVarToggle(Row, TEXT("Cube Log"), TEXT("ZL.Debug.CubeLog"));

	AddHeader(TEXT("Stats (toggle)"));
	Row = AddRow();
	AddButton(Row, TEXT("stat fps"), Console(TEXT("stat fps")));
	AddButton(Row, TEXT("stat unit"), Console(TEXT("stat unit")));
	AddButton(Row, TEXT("stat net"), Console(TEXT("stat net")));

	// Packet lag / loss on this machine's outgoing traffic (engine profiles in BaseEngine.ini [PacketSimulationProfile.*]).
	AddHeader(TEXT("Network Emulation"));
	Row = AddRow();
	for (const TCHAR* Profile : { TEXT("Off"), TEXT("Average"), TEXT("Bad"), TEXT("BufferBloat") })
	{
		const FString Name = Profile;
		UZL_CheatButton* Button = AddButton(Row, Name, [WeakThis, Name]()
		{
			if (UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr)
			{
				Cheats->SetNetEmulationProfile(Name);
			}
		});
		AddLiveLabel(Cast<UTextBlock>(Button->GetContent()), [Name]() { return Name; }, [WeakThis, Name]()
		{
			const UZL_CheatComponent* Cheats = WeakThis.IsValid() ? WeakThis->GetCheats() : nullptr;
			return Cheats && Cheats->NetEmulationProfile == Name;
		});
	}
}

void UZL_CheatMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Toggles change on the server; show the replicated state.
	for (const FLiveLabel& Label : LiveLabels)
	{
		if (UTextBlock* Text = Label.Text.Get())
		{
			if (Label.GetText)
			{
				Text->SetText(FText::FromString(Label.GetText()));
			}
			if (Label.IsOn)
			{
				Text->SetColorAndOpacity(FSlateColor(Label.IsOn() ? ZLCheatMenuStyle::Header : FLinearColor::White));
			}
		}
	}
}

UWidget* UZL_CheatMenu::NativeGetDesiredFocusTarget() const
{
	return FirstButton ? FirstButton.Get() : const_cast<UZL_CheatMenu*>(this);
}

FReply UZL_CheatMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ToggleKey.IsValid() && InKeyEvent.GetKey() == ToggleKey && !InKeyEvent.IsRepeat() && IsActivated())
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UZL_CheatMenu::CloseMenu()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

TOptional<FUIInputConfig> UZL_CheatMenu::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UZL_CheatMenu::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
	}
}

void UZL_CheatMenu::NativeOnDeactivated()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
	}
	Super::NativeOnDeactivated();
}
