//Copyright Preetham Mukundan (C) 2026


#include "UI/HUD/ZL_PlayerInfoBox.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GAS/BaseCharAttributeSet.h"
#include "Gamemode/Zero_BaseGameInstance.h"
#include "MVVMSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/MVVM/GameStats/ZL_VM_PlayerInfo.h"
#include "UI/MVVM/ZL_VM_Attributes.h"
#include "View/MVVMView.h"
#include "Zero_BasePlayerState.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace ZLPlayerCard
{
	static FSlateBrush MakeSolidBrush(float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = 0.f;
		return Brush;
	}
}

void UZL_PlayerInfoBox::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildCard();
		ApplyHero(nullptr);
	}
	bHasShownHero = false;
}

void UZL_PlayerInfoBox::BuildCard()
{
	using namespace ZLPlayerCard;

	const float CardWidth = PortraitSize + FrameThickness * 2.f;

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Root->SetWidthOverride(CardWidth);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->AddChild(Column);

	// Portrait: team frame -> square -> hero colour fill -> hero icon.
	PortraitFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PortraitFrame"));
	PortraitFrame->SetBrush(MakeSolidBrush(CornerRadius));
	PortraitFrame->SetPadding(FMargin(FrameThickness));
	PortraitFrame->SetHorizontalAlignment(HAlign_Fill);
	PortraitFrame->SetVerticalAlignment(VAlign_Fill);
	Column->AddChildToVerticalBox(PortraitFrame);

	USizeBox* Square = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Square->SetWidthOverride(PortraitSize);
	Square->SetHeightOverride(PortraitSize);
	PortraitFrame->SetContent(Square);

	PortraitBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PortraitBackground"));
	PortraitBackground->SetBrush(MakeSolidBrush(FMath::Max(0.f, CornerRadius - FrameThickness)));
	PortraitBackground->SetPadding(FMargin(2.f));
	PortraitBackground->SetHorizontalAlignment(HAlign_Fill);
	PortraitBackground->SetVerticalAlignment(VAlign_Fill);
	Square->AddChild(PortraitBackground);

	PlayerIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PlayerIcon"));
	PortraitBackground->SetContent(PlayerIcon);

	// Health bar: exactly the card width, directly under the portrait.
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarSize->SetHeightOverride(HealthBarHeight);
	if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(BarSize))
	{
		BarSlot->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	FProgressBarStyle BarStyle;
	FSlateBrush BarBackground = MakeSolidBrush(1.f);
	BarBackground.TintColor = FSlateColor(HealthBackgroundColor);
	BarStyle.SetBackgroundImage(BarBackground);
	BarStyle.SetFillImage(MakeSolidBrush(1.f));
	BarStyle.SetMarqueeImage(MakeSolidBrush(1.f));
	HealthBar->SetWidgetStyle(BarStyle);
	HealthBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	HealthBar->SetPercent(1.f);
	BarSize->AddChild(HealthBar);

	if (bShowName)
	{
		PlayerName = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PlayerName"));
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", NameFontSize);
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);
		PlayerName->SetFont(Font);
		PlayerName->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.f)));
		PlayerName->SetJustification(ETextJustify::Center);
		PlayerName->SetClipping(EWidgetClipping::ClipToBounds);
		if (UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(PlayerName))
		{
			NameSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UZL_PlayerInfoBox::SetViewModel(UZL_VM_PlayerInfo* InViewModel)
{
	myVM = InViewModel;
	if (!myVM) return;

	// Widget BP subclasses with an MVVM view still get the viewmodel; the built card doesn't need one.
	if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
	{
		if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
		{
			WidgetView->SetViewModel(FName("ZL_VM_PlayerInfo"), myVM);
		}
	}
	SetAttributeVM(myVM->GetVM_Attributes());

	ShownPawnClass = nullptr;
	bHasShownHero = false;
	ShownHealthPercent = -1.f;
	ShownName.Reset();
	ShownKDA = FIntVector(-1);
	Refresh();
}

void UZL_PlayerInfoBox::SetAttributeVM(UZL_VM_Attributes* AtVM)
{
	myAttributes = AtVM;
	if (!myAttributes) return;
	if (UMVVMSubsystem* MVVMSubsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>())
	{
		if (UMVVMView* WidgetView = MVVMSubsystem->GetViewFromUserWidget(this))
		{
			WidgetView->SetViewModel(FName("ZL_VM_Attributes"), myAttributes);
		}
	}
}

void UZL_PlayerInfoBox::SetIsAlly(bool bInIsAlly)
{
	bIsAlly = bInIsAlly;
	ApplyColors();
}

void UZL_PlayerInfoBox::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

void UZL_PlayerInfoBox::Refresh()
{
	AZero_BasePlayerState* PS = myVM ? myVM->GetOwningPlayerState() : nullptr;
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;

	// Hero portrait follows the pawn's class (hero swap gives a new pawn of another class).
	if (Pawn && Pawn->GetClass() != ShownPawnClass.Get())
	{
		ShownPawnClass = Pawn->GetClass();
		ApplyHero(FindHeroData(this, Pawn->GetClass()));
		bHasShownHero = true;
	}

	// Health straight from the ASC: attributes replicate to every client, the VM may not be fed for remote pawns.
	float Percent = 0.f;
	bool bDead = true;
	if (UAbilitySystemComponent* ASC = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr)
	{
		bool bFoundHealth = false;
		bool bFoundMax = false;
		const float Health = ASC->GetGameplayAttributeValue(UBaseCharAttributeSet::GetCurrentHealthAttribute(), bFoundHealth);
		const float MaxHealth = ASC->GetGameplayAttributeValue(UBaseCharAttributeSet::GetMaximumHealthAttribute(), bFoundMax);
		if (bFoundHealth && bFoundMax && MaxHealth > 0.f)
		{
			Percent = FMath::Clamp(Health / MaxHealth, 0.f, 1.f);
			bDead = Health <= 0.f;
		}
		else
		{
			Percent = 1.f; // attributes not replicated yet
			bDead = false;
		}
	}
	else if (!PS && myAttributes && myAttributes->GetMaxHealth() > 0.f)
	{
		Percent = FMath::Clamp(myAttributes->GetHealth() / myAttributes->GetMaxHealth(), 0.f, 1.f);
		bDead = Percent <= 0.f;
	}

	if (HealthBar && !FMath::IsNearlyEqual(Percent, ShownHealthPercent, 0.001f))
	{
		ShownHealthPercent = Percent;
		HealthBar->SetPercent(Percent);
	}
	if (bDead != bShownDead)
	{
		bShownDead = bDead;
		ApplyColors();
	}

	const FString Name = PS ? PS->GetPlayerName() : (myVM ? myVM->GetPlayerName().ToString() : FString());
	if (PlayerName && Name != ShownName)
	{
		ShownName = Name;
		PlayerName->SetText(FText::FromString(Name));
	}

	const FIntVector KDA = PS ? FIntVector(PS->Kills, PS->Deaths, PS->Assists)
		: (myVM ? FIntVector(myVM->GetKills(), myVM->GetDeaths(), myVM->GetAssists()) : FIntVector::ZeroValue);
	if (KDA != ShownKDA)
	{
		ShownKDA = KDA;
		if (Kill) Kill->SetText(FText::AsNumber(KDA.X));
		if (Death) Death->SetText(FText::AsNumber(KDA.Y));
		if (Assist) Assist->SetText(FText::AsNumber(KDA.Z));
	}
}

void UZL_PlayerInfoBox::ApplyHero(const UZL_Character_Data_Asset* HeroData)
{
	UTexture2D* Icon = (HeroData && HeroData->Icon) ? HeroData->Icon.Get() : DefaultHeroIcon.Get();
	// WBP_PlayerInfo binds the VM's Icon to PlayerIcon (MakeBrushFromTexture); keep it in sync or the binding
	// paints a null, zero-size brush over the portrait.
	if (myVM && Icon)
	{
		myVM->SetIcon(Icon);
	}
	if (PlayerIcon)
	{
		if (Icon)
		{
			PlayerIcon->SetBrushFromTexture(Icon, false);
			PlayerIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (PortraitFrame != nullptr)
		{
			PlayerIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (HeroData)
	{
		HeroPortraitColor = FLinearColor(HeroData->IconColor) * 0.55f;
		HeroPortraitColor.A = 1.f;
	}
	else
	{
		HeroPortraitColor = DefaultPortraitColor;
	}
	ApplyColors();
}

void UZL_PlayerInfoBox::ApplyColors()
{
	const FLinearColor Team = bIsAlly ? AllyColor : EnemyColor;
	if (PortraitFrame)
	{
		PortraitFrame->SetBrushColor(bShownDead ? DeadColor : Team);
	}
	if (PortraitBackground)
	{
		FLinearColor Fill = HeroPortraitColor;
		if (bShownDead)
		{
			Fill = FLinearColor(Fill.GetLuminance() * 0.5f, Fill.GetLuminance() * 0.5f, Fill.GetLuminance() * 0.5f, Fill.A);
		}
		PortraitBackground->SetBrushColor(Fill);
	}
	if (PlayerIcon)
	{
		PlayerIcon->SetColorAndOpacity(bShownDead ? DeadColor : FLinearColor::White);
	}
	// A designer layout (WBP_PlayerInfo) keeps its own bar style.
	if (HealthBar && PortraitFrame != nullptr)
	{
		HealthBar->SetFillColorAndOpacity(bIsAlly ? AllyHealthColor : EnemyHealthColor);
	}
}

UZL_Character_Data_Asset* UZL_PlayerInfoBox::FindHeroData(const UObject* WorldContext, const UClass* PawnClass)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UZero_BaseGameInstance* GI = World ? World->GetGameInstance<UZero_BaseGameInstance>() : nullptr;
	if (!PawnClass || !GI || !GI->CharacterDataTable)
	{
		return nullptr;
	}

	// Closest parent wins, so a BP child of a listed hero still gets that hero's portrait.
	UZL_Character_Data_Asset* Best = nullptr;
	int32 BestDistance = MAX_int32;
	TArray<FHeroTableRow*> Rows;
	GI->CharacterDataTable->GetAllRows<FHeroTableRow>(TEXT("ZL_PlayerInfoBox"), Rows);
	for (const FHeroTableRow* Row : Rows)
	{
		UZL_Character_Data_Asset* Hero = Row ? Row->HeroAsset.LoadSynchronous() : nullptr;
		const UClass* HeroClass = Hero ? Hero->CharacterClass.Get() : nullptr;
		if (!HeroClass || !PawnClass->IsChildOf(HeroClass))
		{
			continue;
		}
		int32 Distance = 0;
		for (const UClass* It = PawnClass; It && It != HeroClass; It = It->GetSuperClass())
		{
			++Distance;
		}
		if (Distance < BestDistance)
		{
			Best = Hero;
			BestDistance = Distance;
		}
	}
	return Best;
}
