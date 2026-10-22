// Copyright Preetham Mukundan (C) 2026

#include "HeroWorkbench/SZL_HeroWorkbench.h"

#include "Editor.h"
#include "FileHelpers.h"
#include "GameplayEffect.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "Engine/Blueprint.h"
#include "Engine/CurveTable.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Engine/Texture2D.h"
#include "GAS/BaseGameplayAbility.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "ZeroLock/ZeroLockCharacter.h"

#define LOCTEXT_NAMESPACE "SZL_HeroWorkbench"

const FName SZL_HeroWorkbench::TabName(TEXT("ZeroLockHeroWorkbench"));

namespace
{
	const FName ColSlot(TEXT("Slot"));
	const FName ColAbility(TEXT("Ability"));
	const FName ColCooldown(TEXT("Cooldown"));
	const FName ColTags(TEXT("Tags"));
	const FName ColIssues(TEXT("Issues"));
	const FName ColSeverity(TEXT("Severity"));
	const FName ColHero(TEXT("Hero"));
	const FName ColContext(TEXT("Context"));
	const FName ColMessage(TEXT("Message"));
	const FName ColAttribute(TEXT("Attribute"));
	const FName ColOp(TEXT("Op"));
	const FName ColValue(TEXT("Value"));
	const FName ColRow(TEXT("Row"));
	const FName ColUsedBy(TEXT("UsedBy"));
	const FName ColDelete(TEXT("Delete"));
	const FName ColProperty(TEXT("Property"));
	const FName ColSource(TEXT("Source"));

	/** Ability levels shown in the curve table grid (0 = not upgraded, then three upgrades). */
	constexpr int32 CurveLevels = 4;

	FName LevelColumn(int32 Level)
	{
		return FName(*FString::Printf(TEXT("Level%d"), Level));
	}

	/** The modifier's scalable float magnitude (protected in FGameplayEffectModifierMagnitude, so by reflection). */
	FScalableFloat* GetModifierScalableFloat(const FGameplayModifierInfo& Modifier)
	{
		static const FStructProperty* Property = FindFProperty<FStructProperty>(FGameplayEffectModifierMagnitude::StaticStruct(), TEXT("ScalableFloatMagnitude"));
		return Property ? const_cast<FScalableFloat*>(Property->ContainerPtrToValuePtr<FScalableFloat>(&Modifier.ModifierMagnitude)) : nullptr;
	}

	/** Multi-column row whose cells come from a callback, so each list doesn't need its own row class. */
	template <typename ItemType>
	class SZL_ColumnRow : public SMultiColumnTableRow<TSharedPtr<ItemType>>
	{
	public:
		using FMakeColumn = TFunction<TSharedRef<SWidget>(const FName&, const TSharedPtr<ItemType>&)>;

		SLATE_BEGIN_ARGS(SZL_ColumnRow) {}
			SLATE_ARGUMENT(TSharedPtr<ItemType>, Item)
			SLATE_ARGUMENT(FMakeColumn, MakeColumn)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& Owner)
		{
			Item = InArgs._Item;
			MakeColumn = InArgs._MakeColumn;
			SMultiColumnTableRow<TSharedPtr<ItemType>>::Construct(typename SMultiColumnTableRow<TSharedPtr<ItemType>>::FArguments(), Owner);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			return SNew(SBox).Padding(FMargin(4.f, 2.f)).VAlign(VAlign_Center)[MakeColumn(Column, Item)];
		}

	private:
		TSharedPtr<ItemType> Item;
		FMakeColumn MakeColumn;
	};

	FString CategoryOf(const FProperty& Property)
	{
		return Property.GetMetaData(TEXT("Category"));
	}

	FText NameText(const UObject* Object)
	{
		if (!Object)
		{
			return LOCTEXT("None", "None");
		}
		if (const UBlueprint* BP = ZLHeroAudit::GetBlueprint(Cast<UClass>(Object)))
		{
			return FText::FromString(BP->GetName());
		}
		return FText::FromString(Object->GetName());
	}

	/** The asset to open for an object: a class's Blueprint, a CDO subobject's Blueprint, else the object. */
	UObject* EditableAsset(UObject* Object)
	{
		if (!Object)
		{
			return nullptr;
		}
		if (UBlueprint* BP = ZLHeroAudit::GetBlueprint(Cast<UClass>(Object)))
		{
			return BP;
		}
		for (UObject* Outer = Object; Outer; Outer = Outer->GetOuter())
		{
			if (Outer->HasAnyFlags(RF_ClassDefaultObject))
			{
				if (UBlueprint* BP = ZLHeroAudit::GetBlueprint(Outer->GetClass()))
				{
					return BP;
				}
			}
		}
		return Object;
	}
}

void SZL_HeroWorkbench::Construct(const FArguments& InArgs)
{
	DataDetails = MakeDetails();
	SlotAssignDetails = MakeDetails([](const FProperty& Property)
	{
		const FClassProperty* ClassProp = CastField<FClassProperty>(&Property);
		const FArrayProperty* ArrayProp = CastField<FArrayProperty>(&Property);
		if (ArrayProp)
		{
			ClassProp = CastField<FClassProperty>(ArrayProp->Inner);
		}
		return ClassProp && ClassProp->MetaClass && ClassProp->MetaClass->IsChildOf(UGameplayAbility::StaticClass());
	});
	AbilityDetails = MakeDetails();
	HeroAnimDetails = MakeDetails([](const FProperty& Property)
	{
		static const TSet<FName> Names = { TEXT("DeadlockAnimSet"), TEXT("LightMeleeMontage"), TEXT("HeavyMeleeMontage"), TEXT("Icon") };
		return Names.Contains(Property.GetFName());
	});
	MeshDetails = MakeDetails([](const FProperty& Property)
	{
		const FString Category = CategoryOf(Property);
		return Category.StartsWith(TEXT("Mesh")) || Category.StartsWith(TEXT("Animation")) || Category.StartsWith(TEXT("Transform")) || Category.StartsWith(TEXT("Materials"));
	});
	AnimSetDetails = MakeDetails();
	FireDetails = MakeDetails([](const FProperty& Property)
	{
		const FString Category = CategoryOf(Property);
		return Category.StartsWith(TEXT("Weapon")) || Category.StartsWith(TEXT("Cooldown")) || Category.StartsWith(TEXT("Costs"));
	});
	ProjectileDetails = MakeDetails([](const FProperty& Property)
	{
		const FString Category = CategoryOf(Property);
		return Category.StartsWith(TEXT("Projectile")) && Property.GetFName() != TEXT("ProjectileMovement");
	});
	ProjectileMoveDetails = MakeDetails([](const FProperty& Property)
	{
		static const TSet<FName> Names = { TEXT("InitialSpeed"), TEXT("MaxSpeed"), TEXT("ProjectileGravityScale"), TEXT("bShouldBounce"),
			TEXT("Bounciness"), TEXT("Friction"), TEXT("bRotationFollowsVelocity"), TEXT("bIsHomingProjectile"), TEXT("HomingAccelerationMagnitude") };
		return Names.Contains(Property.GetFName());
	});
	StatsDetails = MakeDetails([](const FProperty& Property)
	{
		return Property.GetFName() == TEXT("Modifiers");
	});

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		+ SSplitter::Slot()
		.Value(0.22f)
		[
			BuildHeroPanel()
		]
		+ SSplitter::Slot()
		.Value(0.78f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.f)
			[
				BuildHeroHeader()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.f, 0.f, 6.f, 6.f)
			[
				SNew(SSegmentedControl<EZL_WorkbenchPage>)
				.Value_Lambda([this]() { return Page; })
				.OnValueChanged_Lambda([this](EZL_WorkbenchPage NewPage)
				{
					Page = NewPage;
					if (Page == EZL_WorkbenchPage::Audit && !bProjectAuditRun)
					{
						RunProjectAudit();
					}
				})
				+ SSegmentedControl<EZL_WorkbenchPage>::Slot(EZL_WorkbenchPage::Overview).Text(LOCTEXT("Overview", "Overview"))
				+ SSegmentedControl<EZL_WorkbenchPage>::Slot(EZL_WorkbenchPage::Abilities).Text(LOCTEXT("Abilities", "Abilities"))
				+ SSegmentedControl<EZL_WorkbenchPage>::Slot(EZL_WorkbenchPage::Mesh).Text(LOCTEXT("Mesh", "Mesh & Anim"))
				+ SSegmentedControl<EZL_WorkbenchPage>::Slot(EZL_WorkbenchPage::Weapon).Text(LOCTEXT("Weapon", "Weapon"))
				+ SSegmentedControl<EZL_WorkbenchPage>::Slot(EZL_WorkbenchPage::Stats).Text(LOCTEXT("Stats", "Base Stats"))
				+ SSegmentedControl<EZL_WorkbenchPage>::Slot(EZL_WorkbenchPage::Curves).Text(LOCTEXT("Curves", "Curve Table"))
				+ SSegmentedControl<EZL_WorkbenchPage>::Slot(EZL_WorkbenchPage::Audit).Text(LOCTEXT("Audit", "Project Audit"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(6.f, 0.f, 6.f, 6.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SWidgetSwitcher)
					.Visibility_Lambda([this]() { return SelectedHero.IsValid() || Page == EZL_WorkbenchPage::Audit ? EVisibility::Visible : EVisibility::Hidden; })
					.WidgetIndex_Lambda([this]() { return static_cast<int32>(Page); })
					+ SWidgetSwitcher::Slot()[BuildOverviewPage()]
					+ SWidgetSwitcher::Slot()[BuildAbilitiesPage()]
					+ SWidgetSwitcher::Slot()[BuildMeshPage()]
					+ SWidgetSwitcher::Slot()[BuildWeaponPage()]
					+ SWidgetSwitcher::Slot()[BuildStatsPage()]
					+ SWidgetSwitcher::Slot()[BuildCurvesPage()]
					+ SWidgetSwitcher::Slot()[BuildAuditPage()]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return SelectedHero.IsValid() || Page == EZL_WorkbenchPage::Audit ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text(LOCTEXT("PickHero", "Pick a hero on the left"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		]
	];

	RefreshHeroes();
}

// ---------------------------------------------------------------------------------------------------------------------
// Layout

TSharedRef<SWidget> SZL_HeroWorkbench::BuildHeroPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("SearchHeroes", "Search heroes"))
					.OnTextChanged_Lambda([this](const FText& Text)
					{
						SearchText = Text.ToString();
						ApplyHeroFilter();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Rescan", "Rescan"))
					.ToolTipText(LOCTEXT("RescanTip", "Reload the hero table and re-run every hero's checks"))
					.OnClicked_Lambda([this]()
					{
						RefreshHeroes();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f, 4.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bShowUnlisted ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bShowUnlisted = State == ECheckBoxState::Checked;
					RefreshHeroes();
				})
				.ToolTipText(LOCTEXT("UnlistedTip", "Also list hero Blueprints that aren't in the hero table (old test characters, WIP heroes)"))
				[
					SNew(STextBlock).Text(LOCTEXT("ShowUnlisted", "Show heroes not in the hero list"))
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(HeroList, SListView<FHeroPtr>)
				.ListItemsSource(&FilteredHeroes)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SZL_HeroWorkbench::MakeHeroRow)
				.OnSelectionChanged_Lambda([this](FHeroPtr Hero, ESelectInfo::Type) { SelectHero(Hero); })
				.OnMouseButtonDoubleClick_Lambda([this](FHeroPtr Hero)
				{
					OpenAsset(Hero.IsValid() ? Hero->Blueprint.Get() : nullptr);
				})
			]
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildHeroHeader()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(48.f)
			.HeightOverride(48.f)
			[
				SNew(SImage)
				.Image_Lambda([this]() -> const FSlateBrush*
				{
					UTexture2D* Icon = SelectedHero.IsValid() ? SelectedHero->Icon.Get() : nullptr;
					return Icon ? GetBrush(Icon, 48.f) : FAppStyle::GetBrush("ClassIcon.Character");
				})
				.ColorAndOpacity_Lambda([this]() { return SelectedHero.IsValid() && !SelectedHero->Icon.IsValid() ? FSlateColor(SelectedHero->Color) : FSlateColor(FLinearColor::White); })
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
				.Text_Lambda([this]()
				{
					return SelectedHero.IsValid() ? FText::FromString(SelectedHero->Name) : (Page == EZL_WorkbenchPage::Audit ? LOCTEXT("ProjectAudit", "Project Audit") : LOCTEXT("NoHero", "No hero selected"));
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text_Lambda([this]()
				{
					if (!SelectedHero.IsValid())
					{
						return FText::GetEmpty();
					}
					const UBlueprint* BP = SelectedHero->Blueprint.Get();
					return FText::Format(LOCTEXT("HeroSubtitle", "{0}   |   {1} errors, {2} warnings{3}"),
						FText::FromString(BP ? BP->GetPathName() : FString(TEXT("(no hero class)"))),
						SelectedHero->CountIssues(EZL_AuditSeverity::Error), SelectedHero->CountIssues(EZL_AuditSeverity::Warning),
						SelectedHero->bInHeroList ? FText::GetEmpty() : LOCTEXT("NotListed", "   |   not in the hero list"));
				})
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f)
		[
			MakeAssetButton(LOCTEXT("OpenHeroBP", "Open Blueprint"), [this]() -> UObject* { return SelectedHero.IsValid() ? SelectedHero->Blueprint.Get() : nullptr; })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f)
		[
			MakeAssetButton(LOCTEXT("OpenData", "Open Data Asset"), [this]() -> UObject* { return SelectedHero.IsValid() ? SelectedHero->Data.Get() : nullptr; })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Browse", "Find in Content Browser"))
			.IsEnabled_Lambda([this]() { return SelectedHero.IsValid() && SelectedHero->Blueprint.IsValid(); })
			.OnClicked_Lambda([this]()
			{
				BrowseToAsset(SelectedHero.IsValid() ? SelectedHero->Blueprint.Get() : nullptr);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Save", "Save..."))
			.ToolTipText(LOCTEXT("SaveTip", "Save modified assets (shows the usual save dialog)"))
			.OnClicked_Lambda([]()
			{
				FEditorFileUtils::SaveDirtyPackages(true, false, true);
				return FReply::Handled();
			})
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::MakeSection(const FText& Title, const TSharedRef<SWidget>& Content)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f, 2.f, 2.f, 4.f)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).Text(Title)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				Content
			]
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::MakeAssetButton(const FText& Label, TFunction<UObject*()> GetAsset)
{
	return SNew(SButton)
		.Text(Label)
		.IsEnabled_Lambda([GetAsset]() { return GetAsset() != nullptr; })
		.OnClicked_Lambda([GetAsset]()
		{
			OpenAsset(GetAsset());
			return FReply::Handled();
		});
}

TSharedRef<IDetailsView> SZL_HeroWorkbench::MakeDetails(TFunction<bool(const FProperty&)> Filter)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bHideSelectionTip = true;
	Args.bAllowSearch = true;
	Args.bUpdatesFromSelection = false;
	Args.bLockable = false;
	Args.bShowPropertyMatrixButton = false;
	TSharedRef<IDetailsView> View = PropertyEditor.CreateDetailView(Args);
	if (Filter)
	{
		// A struct member is visible when its top-level property is. Component properties (and what they expand to) are
		// hidden: they'd drag in transforms and rendering settings.
		View->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([Filter](const FPropertyAndParent& PropertyAndParent)
		{
			auto IsComponent = [](const FProperty* Property)
			{
				const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property);
				return Object && Object->PropertyClass && Object->PropertyClass->IsChildOf(UActorComponent::StaticClass());
			};
			if (IsComponent(&PropertyAndParent.Property))
			{
				return false;
			}
			for (const FProperty* Parent : PropertyAndParent.ParentProperties)
			{
				if (IsComponent(Parent))
				{
					return false;
				}
			}
			if (Filter(PropertyAndParent.Property))
			{
				return true;
			}
			for (const FProperty* Parent : PropertyAndParent.ParentProperties)
			{
				if (Parent && Filter(*Parent))
				{
					return true;
				}
			}
			return false;
		}));
	}
	View->OnFinishedChangingProperties().AddSP(this, &SZL_HeroWorkbench::OnDetailsChanged, TWeakPtr<IDetailsView>(View));
	return View;
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildOverviewPage()
{
	return SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		+ SSplitter::Slot()
		.Value(0.5f)
		[
			MakeSection(LOCTEXT("HeroData", "Hero select data (data asset)"),
				SNew(SOverlay)
				+ SOverlay::Slot()[DataDetails.ToSharedRef()]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return SelectedHero.IsValid() && !SelectedHero->Data.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("NoData", "Not in the hero table: no data asset.\nAdd a row to DT_HeroList to make it pickable."))
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				])
		]
		+ SSplitter::Slot()
		.Value(0.5f)
		[
			MakeSection(LOCTEXT("HeroIssues", "Checks (double-click to open the asset)"), BuildIssueList(HeroIssueList, &HeroIssueItems, false))
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildAbilitiesPage()
{
	return SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+ SSplitter::Slot()
		.Value(0.42f)
		[
			MakeSection(LOCTEXT("Slots", "Ability slots (select one to edit it below, double-click to open its Blueprint)"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(SlotList, SListView<FSlotPtr>)
					.ListItemsSource(&SlotItems)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow_Lambda([this](FSlotPtr Slot, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
					{
						return SNew(SZL_ColumnRow<FZL_AbilitySlot>, Owner).Item(Slot).MakeColumn([this](const FName& Column, const FSlotPtr& Item) { return MakeSlotColumn(Column, Item); });
					})
					.OnSelectionChanged(this, &SZL_HeroWorkbench::OnSlotSelected)
					.OnMouseButtonDoubleClick_Lambda([](FSlotPtr Slot)
					{
						OpenAsset(Slot.IsValid() ? EditableAsset(Slot->AbilityClass.Get()) : nullptr);
					})
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(ColSlot).DefaultLabel(LOCTEXT("ColSlot", "Slot")).FillWidth(0.16f)
						+ SHeaderRow::Column(ColAbility).DefaultLabel(LOCTEXT("ColAbility", "Ability")).FillWidth(0.26f)
						+ SHeaderRow::Column(ColCooldown).DefaultLabel(LOCTEXT("ColCooldown", "Cooldown")).FillWidth(0.08f)
						+ SHeaderRow::Column(ColTags).DefaultLabel(LOCTEXT("ColTags", "Cooldown tags")).FillWidth(0.2f)
						+ SHeaderRow::Column(ColIssues).DefaultLabel(LOCTEXT("ColIssues", "Issues")).FillWidth(0.3f)
					)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(true)
					.AreaTitle(LOCTEXT("AssignSlots", "Assign abilities to slots (hero Blueprint)"))
					.BodyContent()
					[
						SNew(SBox)
						.MaxDesiredHeight(320.f)
						[
							SlotAssignDetails.ToSharedRef()
						]
					]
				])
		]
		+ SSplitter::Slot()
		.Value(0.58f)
		[
			MakeSection(LOCTEXT("AbilityDefaults", "Selected ability (class defaults)"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						MakeAssetButton(LOCTEXT("OpenAbility", "Open ability Blueprint"), [this]() -> UObject*
						{
							const TArray<FSlotPtr> Selected = SlotList.IsValid() ? SlotList->GetSelectedItems() : TArray<FSlotPtr>();
							return Selected.Num() > 0 ? EditableAsset(Selected[0]->AbilityClass.Get()) : nullptr;
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("BrowseAbility", "Find in Content Browser"))
						.OnClicked_Lambda([this]()
						{
							const TArray<FSlotPtr> Selected = SlotList.IsValid() ? SlotList->GetSelectedItems() : TArray<FSlotPtr>();
							BrowseToAsset(Selected.Num() > 0 ? EditableAsset(Selected[0]->AbilityClass.Get()) : nullptr);
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					AbilityDetails.ToSharedRef()
				])
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildMeshPage()
{
	return SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		+ SSplitter::Slot()
		.Value(0.5f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.65f)
			[
				MakeSection(LOCTEXT("MeshComp", "Mesh component (mesh, anim class, transform)"), MeshDetails.ToSharedRef())
			]
			+ SSplitter::Slot()
			.Value(0.35f)
			[
				MakeSection(LOCTEXT("HeroAnim", "Hero animation settings"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeAssetButton(LOCTEXT("OpenMesh", "Open mesh"), [this]() -> UObject*
							{
								const UObject* Mesh = GetMeshComponent();
								return Mesh ? ZLHeroAudit::GetObjectProperty(Mesh, TEXT("SkinnedAsset")) : nullptr;
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.f, 0.f, 0.f, 0.f)
						[
							MakeAssetButton(LOCTEXT("OpenAnimBP", "Open anim Blueprint"), [this]() -> UObject*
							{
								const UObject* Mesh = GetMeshComponent();
								return Mesh ? EditableAsset(ZLHeroAudit::GetClassProperty(Mesh, TEXT("AnimClass"))) : nullptr;
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.f, 0.f, 0.f, 0.f)
						[
							MakeAssetButton(LOCTEXT("OpenAnimSet", "Open anim set"), [this]() { return GetAnimSet(); })
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						HeroAnimDetails.ToSharedRef()
					])
			]
		]
		+ SSplitter::Slot()
		.Value(0.5f)
		[
			MakeSection(LOCTEXT("AnimSet", "Deadlock anim set"),
				SNew(SOverlay)
				+ SOverlay::Slot()[AnimSetDetails.ToSharedRef()]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return GetAnimSet() ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text(LOCTEXT("NoAnimSet", "No Deadlock anim set on this hero"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				])
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildWeaponPage()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([this]() { return GetWeaponSummary(); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					MakeAssetButton(LOCTEXT("OpenFire", "Open fire ability"), [this]() { return EditableAsset(GetFireCDO()); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					MakeAssetButton(LOCTEXT("OpenProjectile", "Open projectile"), [this]() { return EditableAsset(GetProjectileCDO()); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					MakeAssetButton(LOCTEXT("OpenReload", "Open reload ability"), [this]() -> UObject*
					{
						const AZeroLockCharacter* CDO = Cast<AZeroLockCharacter>(GetHeroCDO());
						return CDO ? EditableAsset(CDO->ReloadAbility.Get()) : nullptr;
					})
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.5f)
			[
				MakeSection(LOCTEXT("FireAbility", "Primary fire ability"), FireDetails.ToSharedRef())
			]
			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.6f)
				[
					MakeSection(LOCTEXT("Projectile", "Projectile (damage, bounces, FX)"), ProjectileDetails.ToSharedRef())
				]
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					MakeSection(LOCTEXT("ProjectileMove", "Projectile movement"), ProjectileMoveDetails.ToSharedRef())
				]
			]
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildStatsPage()
{
	return SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		+ SSplitter::Slot()
		.Value(0.45f)
		[
			MakeSection(LOCTEXT("StatsSummary", "Base stats (DefaultGameplayEffect modifiers at level 1, click a value to edit it)"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([this]()
					{
						const UObject* Effect = GetDefaultEffectCDO();
						if (!Effect)
						{
							return LOCTEXT("NoGE", "No DefaultGameplayEffect");
						}
						const TArray<FString> Sharing = GetHeroesSharingDefaultEffect();
						const FText Name = NameText(Effect->GetClass());
						return Sharing.Num() == 0
							? FText::Format(LOCTEXT("GEOwn", "{0}: used by this hero only"), Name)
							: FText::Format(LOCTEXT("GEShared", "{0}: shared with {1}. Editing it changes them too."), Name, FText::FromString(FString::Join(Sharing, TEXT(", "))));
					})
					.ColorAndOpacity_Lambda([this]()
					{
						return GetHeroesSharingDefaultEffect().Num() > 0 ? FSlateColor(ZLHeroAudit::SeverityColor(EZL_AuditSeverity::Warning)) : FSlateColor::UseForeground();
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(SWrapBox)
					.UseAllottedSize(true)
					.InnerSlotPadding(FVector2D(4.f, 4.f))
					+ SWrapBox::Slot()
					[
						MakeAssetButton(LOCTEXT("OpenGE", "Open DefaultGameplayEffect"), [this]() { return EditableAsset(GetDefaultEffectCDO()); })
					]
					+ SWrapBox::Slot()
					[
						SNew(SButton)
						.Text(LOCTEXT("CopyGE", "Make a copy for this hero"))
						.ToolTipText(LOCTEXT("CopyGETip", "Duplicates the effect to <hero ability folder>/GE_<Hero>_DefaultStats and assigns it to this hero"))
						.IsEnabled_Lambda([this]() { return GetDefaultEffectCDO() && GetHeroesSharingDefaultEffect().Num() > 0; })
						.OnClicked_Lambda([this]()
						{
							CopyDefaultEffect(false);
							return FReply::Handled();
						})
					]
					+ SWrapBox::Slot()
					[
						SNew(SButton)
						.Text(LOCTEXT("CopyGEAll", "Make a copy for every hero sharing it"))
						.IsEnabled_Lambda([this]() { return GetDefaultEffectCDO() && GetHeroesSharingDefaultEffect().Num() > 0; })
						.OnClicked_Lambda([this]()
						{
							CopyDefaultEffect(true);
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(StatList, SListView<FStatPtr>)
					.ListItemsSource(&StatItems)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateRow_Lambda([this](FStatPtr Stat, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
					{
						return SNew(SZL_ColumnRow<FZL_StatRow>, Owner).Item(Stat).MakeColumn([this](const FName& Column, const FStatPtr& Item) { return MakeStatColumn(Column, Item); });
					})
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(ColAttribute).DefaultLabel(LOCTEXT("ColAttribute", "Attribute")).FillWidth(0.45f)
						+ SHeaderRow::Column(ColOp).DefaultLabel(LOCTEXT("ColOp", "Operation")).FillWidth(0.25f)
						+ SHeaderRow::Column(ColValue).DefaultLabel(LOCTEXT("ColValue", "Value")).FillWidth(0.3f)
					)
				])
		]
		+ SSplitter::Slot()
		.Value(0.55f)
		[
			MakeSection(LOCTEXT("StatsEdit", "Edit modifiers"), StatsDetails.ToSharedRef())
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildCurvesPage()
{
	TSharedRef<SHeaderRow> RowHeader = SNew(SHeaderRow)
		+ SHeaderRow::Column(ColRow).DefaultLabel(LOCTEXT("ColRow", "Row")).FillWidth(0.24f);
	for (int32 Level = 0; Level < CurveLevels; ++Level)
	{
		RowHeader->AddColumn(SHeaderRow::Column(LevelColumn(Level))
			.DefaultLabel(Level == 0 ? LOCTEXT("Level0", "Level 0 (base)") : FText::Format(LOCTEXT("LevelN", "Level {0}"), Level))
			.DefaultTooltip(LOCTEXT("LevelTip", "Ability level: 0 = not upgraded yet, each upgrade adds 1"))
			.FillWidth(0.1f));
	}
	RowHeader->AddColumn(SHeaderRow::Column(ColUsedBy).DefaultLabel(LOCTEXT("ColUsedBy", "Used by")).FillWidth(0.3f));
	RowHeader->AddColumn(SHeaderRow::Column(ColDelete).DefaultLabel(FText::GetEmpty()).FixedWidth(30.f));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							const UCurveTable* Table = CurveTable.Get();
							return Table ? FText::Format(LOCTEXT("CurveTableName", "{0}   ({1})"), FText::FromString(Table->GetName()), FText::FromString(Table->GetPathName()))
								: LOCTEXT("NoCurveTable", "No curve table: this hero's ability values are all constants");
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Visibility_Lambda([this]() { return CurveTable.IsValid() ? EVisibility::Collapsed : EVisibility::Visible; })
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("CreateTable", "Create C_{0}"), FText::FromString(SelectedHero.IsValid() ? ZLHeroAudit::GetHeroShortName(*SelectedHero) : FString()));
						})
						.ToolTipText(LOCTEXT("CreateTableTip", "Creates the hero's curve table in its ability folder"))
						.OnClicked_Lambda([this]()
						{
							CreateCurveTable();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text_Lambda([this]()
						{
							int32 Count = 0;
							for (const FCurveLinkPtr& Link : CurveLinks)
							{
								Count += !Link->Table.IsValid() && !FMath::IsNearlyZero(Link->Value) ? 1 : 0;
							}
							return FText::Format(LOCTEXT("LinkValues", "Move constant values into the table ({0})"), Count);
						})
						.ToolTipText(LOCTEXT("LinkValuesTip", "Each non-zero constant ability value gets a row <Ability>_<Property> with its current value at every level, and the ability reads it from there (Value 1 x row)"))
						.IsEnabled_Lambda([this]() { return CurveTable.IsValid(); })
						.OnClicked_Lambda([this]()
						{
							LinkValuesToCurveTable();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.f, 0.f, 0.f, 0.f)
					[
						MakeAssetButton(LOCTEXT("OpenTable", "Open table editor"), [this]() -> UObject* { return CurveTable.Get(); })
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("NewRow", "New row"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.4f)
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(SEditableTextBox)
						.HintText(LOCTEXT("NewRowHint", "Ability_Stat, e.g. CallBell_ImpactDamage"))
						.Text_Lambda([this]() { return FText::FromString(NewCurveRowName); })
						.OnTextChanged_Lambda([this](const FText& Text) { NewCurveRowName = Text.ToString(); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("NewRowValue", "value"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.15f)
					.Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SNumericEntryBox<float>)
						.Value_Lambda([this]() { return TOptional<float>(NewCurveRowValue); })
						.OnValueChanged_Lambda([this](float Value) { NewCurveRowValue = Value; })
						.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { NewCurveRowValue = Value; })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("AddRow", "Add row"))
						.IsEnabled_Lambda([this]() { return CurveTable.IsValid() && !NewCurveRowName.IsEmpty(); })
						.OnClicked_Lambda([this]()
						{
							AddCurveRow();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.45f)
					.VAlign(VAlign_Center)
					.Padding(12.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text_Lambda([this]() { return CurveStatus; })
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.55f)
			[
				MakeSection(LOCTEXT("CurveRows", "Rows (edit a cell to change that level's value)"),
					SAssignNew(CurveRowList, SListView<FCurveRowPtr>)
					.ListItemsSource(&CurveRows)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateRow_Lambda([this](FCurveRowPtr Row, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
					{
						return SNew(SZL_ColumnRow<FZL_CurveRow>, Owner).Item(Row).MakeColumn([this](const FName& Column, const FCurveRowPtr& Item) { return MakeCurveRowColumn(Column, Item); });
					})
					.HeaderRow(RowHeader))
			]
			+ SSplitter::Slot()
			.Value(0.45f)
			[
				MakeSection(LOCTEXT("CurveLinks", "Ability values (double-click to open the ability)"),
					SAssignNew(CurveLinkList, SListView<FCurveLinkPtr>)
					.ListItemsSource(&CurveLinks)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow_Lambda([this](FCurveLinkPtr Link, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
					{
						return SNew(SZL_ColumnRow<FZL_CurveLink>, Owner).Item(Link).MakeColumn([this](const FName& Column, const FCurveLinkPtr& Item) { return MakeCurveLinkColumn(Column, Item); });
					})
					.OnMouseButtonDoubleClick_Lambda([](FCurveLinkPtr Link)
					{
						OpenAsset(Link.IsValid() ? EditableAsset(Link->AbilityClass.Get()) : nullptr);
					})
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(ColAbility).DefaultLabel(LOCTEXT("ColLinkAbility", "Ability")).FillWidth(0.25f)
						+ SHeaderRow::Column(ColProperty).DefaultLabel(LOCTEXT("ColProperty", "Property")).FillWidth(0.25f)
						+ SHeaderRow::Column(ColSource).DefaultLabel(LOCTEXT("ColSource", "Value comes from")).FillWidth(0.5f)
					))
			]
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildAuditPage()
{
	return MakeSection(LOCTEXT("AuditTitle", "Listed heroes (plus unlisted ones when shown on the left), unused hero abilities, and shipping content that references Developers/Deadlock assets"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RunAudit", "Run audit"))
				.OnClicked_Lambda([this]()
				{
					RefreshHeroes();
					RunProjectAudit();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExportCSV", "Export CSV"))
				.ToolTipText(LOCTEXT("ExportTip", "Writes AbilityAudit.csv, HeroSlots.csv and Issues.csv to Saved/ZeroLockAudit and opens the folder"))
				.OnClicked_Lambda([this]()
				{
					ExportCSV();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bShowInfoIssues ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bShowInfoIssues = State == ECheckBoxState::Checked;
					RefreshProjectIssueItems();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("ShowInfo", "Show info"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text_Lambda([this]() { return ProjectAuditSummary; })
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			BuildIssueList(ProjectIssueList, &ProjectIssueItems, true)
		]);
}

TSharedRef<SWidget> SZL_HeroWorkbench::BuildIssueList(TSharedPtr<SListView<FIssuePtr>>& OutList, TArray<FIssuePtr>* Items, bool bShowHeroColumn)
{
	TSharedRef<SHeaderRow> Header = SNew(SHeaderRow)
		+ SHeaderRow::Column(ColSeverity).DefaultLabel(FText::GetEmpty()).FixedWidth(70.f);
	if (bShowHeroColumn)
	{
		Header->AddColumn(SHeaderRow::Column(ColHero).DefaultLabel(LOCTEXT("ColHero", "Hero")).FillWidth(0.12f));
	}
	Header->AddColumn(SHeaderRow::Column(ColContext).DefaultLabel(LOCTEXT("ColContext", "Where")).FillWidth(0.28f));
	Header->AddColumn(SHeaderRow::Column(ColMessage).DefaultLabel(LOCTEXT("ColMessage", "Issue")).FillWidth(0.6f));

	return SAssignNew(OutList, SListView<FIssuePtr>)
		.ListItemsSource(Items)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow_Lambda([this](FIssuePtr Issue, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
		{
			return SNew(SZL_ColumnRow<FZL_AuditIssue>, Owner).Item(Issue).MakeColumn([this](const FName& Column, const FIssuePtr& Item) { return MakeIssueColumn(Column, Item); });
		})
		.OnMouseButtonDoubleClick_Lambda([](FIssuePtr Issue)
		{
			OpenAsset(Issue.IsValid() ? Issue->Asset.TryLoad() : nullptr);
		})
		.HeaderRow(Header);
}

// ---------------------------------------------------------------------------------------------------------------------
// Rows

TSharedRef<ITableRow> SZL_HeroWorkbench::MakeHeroRow(FHeroPtr Hero, const TSharedRef<STableViewBase>& Owner)
{
	const int32 Errors = Hero->CountIssues(EZL_AuditSeverity::Error);
	const int32 Warnings = Hero->CountIssues(EZL_AuditSeverity::Warning);
	UTexture2D* Icon = Hero->Icon.Get();

	return SNew(STableRow<FHeroPtr>, Owner)
		.Padding(FMargin(2.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(32.f)
				.HeightOverride(32.f)
				[
					SNew(SImage)
					.Image(Icon ? GetBrush(Icon, 32.f) : FAppStyle::GetBrush("ClassIcon.Character"))
					.ColorAndOpacity(Icon ? FSlateColor(FLinearColor::White) : FSlateColor(Hero->Color))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.Text(FText::FromString(Hero->Name))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text(Hero->bInHeroList ? NameText(Hero->HeroClass.Get()) : LOCTEXT("Unlisted", "not in the hero list"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Visibility(Errors > 0 ? EVisibility::Visible : EVisibility::Collapsed)
				.ColorAndOpacity(ZLHeroAudit::SeverityColor(EZL_AuditSeverity::Error))
				.Text(FText::Format(LOCTEXT("ErrorBadge", "{0} E"), Errors))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Visibility(Warnings > 0 ? EVisibility::Visible : EVisibility::Collapsed)
				.ColorAndOpacity(ZLHeroAudit::SeverityColor(EZL_AuditSeverity::Warning))
				.Text(FText::Format(LOCTEXT("WarnBadge", "{0} W"), Warnings))
			]
		];
}

TSharedRef<SWidget> SZL_HeroWorkbench::MakeSlotColumn(const FName& Column, const FSlotPtr& Slot)
{
	const UBaseGameplayAbility* CDO = Slot->AbilityClass ? Slot->AbilityClass->GetDefaultObject<UBaseGameplayAbility>() : nullptr;
	if (Column == ColSlot)
	{
		return SNew(STextBlock).Text(FText::FromString(Slot->Label));
	}
	if (Column == ColAbility)
	{
		UTexture2D* Icon = CDO ? CDO->IconImage.Get() : nullptr;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(20.f)
				.HeightOverride(20.f)
				.Visibility(Icon ? EVisibility::Visible : EVisibility::Hidden)
				[
					SNew(SImage).Image(Icon ? GetBrush(Icon, 20.f) : nullptr)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(CDO && !CDO->AbilityName.IsEmpty()
					? FText::Format(LOCTEXT("AbilityLabel", "{0}  ({1})"), FText::FromString(CDO->AbilityName), NameText(Slot->AbilityClass.Get()))
					: NameText(Slot->AbilityClass.Get()))
				.ColorAndOpacity(CDO ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
			];
	}
	if (Column == ColCooldown)
	{
		return SNew(STextBlock).Text(CDO ? FText::AsNumber(ZLHeroAudit::GetCooldown(CDO)) : FText::GetEmpty());
	}
	if (Column == ColTags)
	{
		return SNew(STextBlock).Text(CDO ? FText::FromString(CDO->CooldownTags.ToStringSimple()) : FText::GetEmpty());
	}
	// Issues: the worst one's colour, all messages in the tooltip.
	if (Slot->Issues.Num() == 0)
	{
		return SNew(STextBlock).Text(CDO ? LOCTEXT("SlotOK", "OK") : FText::GetEmpty()).ColorAndOpacity(FLinearColor(0.4f, 0.8f, 0.4f));
	}
	TArray<FZL_AuditIssue> Sorted = Slot->Issues;
	Sorted.StableSort([](const FZL_AuditIssue& A, const FZL_AuditIssue& B) { return A.Severity > B.Severity; });
	const EZL_AuditSeverity Worst = Sorted[0].Severity;
	TArray<FString> Messages;
	for (const FZL_AuditIssue& Issue : Sorted)
	{
		Messages.Add(Issue.Message);
	}
	const FString Joined = FString::Join(Messages, TEXT("\n"));
	return SNew(STextBlock)
		.Text(FText::FromString(Messages.Num() == 1 ? Messages[0] : FString::Printf(TEXT("%s (+%d more)"), *Messages[0], Messages.Num() - 1)))
		.ToolTipText(FText::FromString(Joined))
		.ColorAndOpacity(ZLHeroAudit::SeverityColor(Worst));
}

TSharedRef<SWidget> SZL_HeroWorkbench::MakeIssueColumn(const FName& Column, const FIssuePtr& Issue)
{
	if (Column == ColSeverity)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(ZLHeroAudit::SeverityToString(Issue->Severity)))
			.ColorAndOpacity(ZLHeroAudit::SeverityColor(Issue->Severity));
	}
	if (Column == ColHero)
	{
		return SNew(STextBlock).Text(FText::FromString(Issue->Hero));
	}
	if (Column == ColContext)
	{
		return SNew(STextBlock).Text(FText::FromString(Issue->Context)).ToolTipText(FText::FromString(Issue->Asset.ToString()));
	}
	return SNew(STextBlock).Text(FText::FromString(Issue->Message)).ToolTipText(FText::FromString(Issue->Message));
}

TSharedRef<SWidget> SZL_HeroWorkbench::MakeStatColumn(const FName& Column, const FStatPtr& Stat)
{
	const FString& Text = Column == ColAttribute ? Stat->Attribute : (Column == ColOp ? Stat->Operation : Stat->Value);
	// StatItems is built one row per modifier, in order.
	const int32 ModifierIndex = StatItems.IndexOfByKey(Stat);
	const UGameplayEffect* Effect = Cast<UGameplayEffect>(GetDefaultEffectCDO());
	if (Column != ColValue || !Effect || !Effect->Modifiers.IsValidIndex(ModifierIndex))
	{
		return SNew(STextBlock).Text(FText::FromString(Text));
	}

	const FGameplayModifierInfo& Modifier = Effect->Modifiers[ModifierIndex];
	const FScalableFloat* Magnitude = GetModifierScalableFloat(Modifier);
	if (!Magnitude || Modifier.ModifierMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::ScalableFloat)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.ToolTipText(LOCTEXT("StatCalculated", "Not a plain number (attribute based, custom calculation or SetByCaller): edit it on the right"));
	}
	if (Magnitude->Curve.CurveTable)
	{
		return SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("StatCurve", "{0}  ({1} x {2}.{3})"), FText::FromString(Text), FText::AsNumber(Magnitude->Value),
				FText::FromString(Magnitude->Curve.CurveTable->GetName()), FText::FromName(Magnitude->Curve.RowName)))
			.ToolTipText(LOCTEXT("StatCurveTip", "Comes from a curve table row: edit the row, or clear the curve on the right"));
	}

	// The CDO is looked up again on every read and write: the details panel or a recompile can replace it.
	return SNew(SNumericEntryBox<float>)
		.ToolTipText(LOCTEXT("StatEditTip", "Edit the modifier's value (writes the effect's class defaults; Save... to keep it)"))
		.Value_Lambda([this, ModifierIndex]() -> TOptional<float>
		{
			const UGameplayEffect* Current = Cast<UGameplayEffect>(GetDefaultEffectCDO());
			const FScalableFloat* Value = Current && Current->Modifiers.IsValidIndex(ModifierIndex) ? GetModifierScalableFloat(Current->Modifiers[ModifierIndex]) : nullptr;
			return Value ? TOptional<float>(Value->Value) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([this, ModifierIndex](float NewValue, ETextCommit::Type)
		{
			UGameplayEffect* Current = Cast<UGameplayEffect>(GetDefaultEffectCDO());
			FScalableFloat* Value = Current && Current->Modifiers.IsValidIndex(ModifierIndex) ? GetModifierScalableFloat(Current->Modifiers[ModifierIndex]) : nullptr;
			if (!Value || FMath::IsNearlyEqual(Value->Value, NewValue))
			{
				return;
			}
			Current->Modify();
			Value->Value = NewValue;
			ZLHeroAudit::MarkClassDefaultsEdited(Current);
			if (StatsDetails.IsValid())
			{
				StatsDetails->ForceRefresh();
			}
			RefreshSelectedHero(false); // re-runs the checks (e.g. CurrentAmmo above MaxAmmo)
		});
}

TSharedRef<SWidget> SZL_HeroWorkbench::MakeCurveRowColumn(const FName& Column, const FCurveRowPtr& Row)
{
	const FName RowName = Row->Row;
	if (Column == ColRow)
	{
		return SNew(STextBlock)
			.Text(FText::FromName(RowName))
			.ColorAndOpacity(Row->UsedBy.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground());
	}
	if (Column == ColUsedBy)
	{
		return SNew(STextBlock)
			.Text(Row->UsedBy.IsEmpty() ? LOCTEXT("RowUnused", "(not used by this hero's abilities)") : FText::FromString(Row->UsedBy))
			.ToolTipText(FText::FromString(Row->UsedBy))
			.ColorAndOpacity(Row->UsedBy.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground());
	}
	if (Column == ColDelete)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("DeleteRow", "Delete this row"))
			.OnClicked_Lambda([this, RowName]()
			{
				DeleteCurveRow(RowName);
				return FReply::Handled();
			})
			[
				SNew(SImage).Image(FAppStyle::GetBrush("Icons.Delete")).ColorAndOpacity(FSlateColor::UseForeground())
			];
	}
	for (int32 Level = 0; Level < CurveLevels; ++Level)
	{
		if (Column == LevelColumn(Level))
		{
			return SNew(SNumericEntryBox<float>)
				.Value_Lambda([this, RowName, Level]() { return TOptional<float>(ZLHeroAudit::GetCurveTableValue(CurveTable.Get(), RowName, Level)); })
				.OnValueCommitted_Lambda([this, RowName, Level](float Value, ETextCommit::Type)
				{
					if (!FMath::IsNearlyEqual(Value, ZLHeroAudit::GetCurveTableValue(CurveTable.Get(), RowName, Level)))
					{
						ZLHeroAudit::SetCurveTableValue(CurveTable.Get(), RowName, Level, Value);
						CurveStatus = FText::Format(LOCTEXT("CellSet", "{0} level {1} = {2} (unsaved)"), FText::FromName(RowName), Level, FText::AsNumber(Value));
					}
				});
		}
	}
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SZL_HeroWorkbench::MakeCurveLinkColumn(const FName& Column, const FCurveLinkPtr& Link)
{
	if (Column == ColAbility)
	{
		return SNew(STextBlock).Text(NameText(Link->AbilityClass.Get()));
	}
	if (Column == ColProperty)
	{
		return SNew(STextBlock).Text(FText::FromName(Link->Property));
	}
	const UCurveTable* Table = Link->Table.Get();
	if (Link->bRowMissing)
	{
		return SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("SourceMissing", "MISSING row '{0}' in {1}: evaluates to 0"), FText::FromName(Link->Row), FText::FromString(Table ? Table->GetName() : FString())))
			.ColorAndOpacity(ZLHeroAudit::SeverityColor(EZL_AuditSeverity::Error));
	}
	if (!Table)
	{
		return SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("SourceConstant", "constant {0}"), FText::AsNumber(Link->Value)))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}
	const bool bOtherTable = Table != CurveTable.Get();
	return SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("SourceRow", "{0} x {1}.{2}"), FText::AsNumber(Link->Value), FText::FromString(Table->GetName()), FText::FromName(Link->Row)))
		.ColorAndOpacity(bOtherTable ? FSlateColor(ZLHeroAudit::SeverityColor(EZL_AuditSeverity::Warning)) : FSlateColor::UseForeground())
		.ToolTipText(bOtherTable ? LOCTEXT("OtherTable", "Reads from another hero's table") : FText::GetEmpty());
}

// ---------------------------------------------------------------------------------------------------------------------
// Behaviour

void SZL_HeroWorkbench::RefreshCurvePage()
{
	CurveRows.Reset();
	CurveLinks.Reset();
	CurveTable = SelectedHero.IsValid() ? ZLHeroAudit::FindHeroCurveTable(*SelectedHero) : nullptr;

	if (SelectedHero.IsValid())
	{
		for (const FZL_CurveLink& Link : ZLHeroAudit::GetHeroScalableFloats(*SelectedHero))
		{
			CurveLinks.Add(MakeShared<FZL_CurveLink>(Link));
		}
	}
	if (const UCurveTable* Table = CurveTable.Get())
	{
		TArray<FName> Names;
		Table->GetRowMap().GetKeys(Names);
		Names.Sort(FNameLexicalLess());
		for (const FName& Name : Names)
		{
			TSharedPtr<FZL_CurveRow> Row = MakeShared<FZL_CurveRow>();
			Row->Row = Name;
			TArray<FString> Users;
			for (const FCurveLinkPtr& Link : CurveLinks)
			{
				if (Link->Table.Get() == Table && Link->Row == Name)
				{
					Users.Add(FString::Printf(TEXT("%s.%s"), *Link->AbilityShortName, *Link->Property.ToString()));
				}
			}
			Row->UsedBy = FString::Join(Users, TEXT(", "));
			CurveRows.Add(Row);
		}
	}
	if (CurveRowList.IsValid())
	{
		CurveRowList->RebuildList();
	}
	if (CurveLinkList.IsValid())
	{
		CurveLinkList->RebuildList();
	}
}

void SZL_HeroWorkbench::CreateCurveTable()
{
	if (!SelectedHero.IsValid())
	{
		return;
	}
	FString Error;
	UCurveTable* Table = ZLHeroAudit::CreateHeroCurveTable(*SelectedHero, Error);
	CurveStatus = Table ? FText::Format(LOCTEXT("TableCreated", "Created {0} (unsaved). Next: move the constant values into it."), FText::FromString(Table->GetName()))
		: FText::FromString(Error);
	RefreshCurvePage();
	CurveTable = Table ? Table : CurveTable.Get();
}

void SZL_HeroWorkbench::LinkValuesToCurveTable()
{
	if (!SelectedHero.IsValid() || !CurveTable.IsValid())
	{
		return;
	}
	const int32 Count = ZLHeroAudit::LinkHeroValuesToTable(*SelectedHero, CurveTable.Get(), CurveLevels);
	CurveStatus = FText::Format(LOCTEXT("Linked", "Moved {0} values into {1} (unsaved: Save... writes the table and the abilities)"), Count, FText::FromString(CurveTable->GetName()));
	RefreshSelectedHero(false);
}

void SZL_HeroWorkbench::AddCurveRow()
{
	const FName Row(*NewCurveRowName.TrimStartAndEnd());
	if (ZLHeroAudit::AddCurveTableRow(CurveTable.Get(), Row, NewCurveRowValue, CurveLevels))
	{
		CurveStatus = FText::Format(LOCTEXT("RowAdded", "Added {0} (unsaved)"), FText::FromName(Row));
		NewCurveRowName.Reset();
	}
	else
	{
		CurveStatus = FText::Format(LOCTEXT("RowExists", "{0} already exists"), FText::FromName(Row));
	}
	RefreshCurvePage();
}

void SZL_HeroWorkbench::DeleteCurveRow(FName Row)
{
	const FCurveRowPtr* Found = CurveRows.FindByPredicate([Row](const FCurveRowPtr& Item) { return Item->Row == Row; });
	const FString UsedBy = Found ? (*Found)->UsedBy : FString();
	const FText Prompt = UsedBy.IsEmpty()
		? FText::Format(LOCTEXT("DeleteRowPrompt", "Delete row {0}?"), FText::FromName(Row))
		: FText::Format(LOCTEXT("DeleteUsedRowPrompt", "Row {0} is used by {1}: those values become 0. Delete it anyway?"), FText::FromName(Row), FText::FromString(UsedBy));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Prompt) != EAppReturnType::Yes)
	{
		return;
	}
	ZLHeroAudit::RemoveCurveTableRow(CurveTable.Get(), Row);
	CurveStatus = FText::Format(LOCTEXT("RowDeleted", "Deleted {0} (unsaved)"), FText::FromName(Row));
	RefreshSelectedHero(false);
}

TArray<FString> SZL_HeroWorkbench::GetHeroesSharingDefaultEffect() const
{
	TArray<FString> Names;
	const AZeroLockCharacter* CDO = Cast<AZeroLockCharacter>(GetHeroCDO());
	if (!CDO || !CDO->DefaultGameplayEffect)
	{
		return Names;
	}
	for (const FHeroPtr& Hero : AllHeroes)
	{
		const AZeroLockCharacter* Other = Hero->GetHeroCDO();
		if (Hero != SelectedHero && Other && Other->DefaultGameplayEffect == CDO->DefaultGameplayEffect)
		{
			Names.Add(Hero->Name);
		}
	}
	return Names;
}

void SZL_HeroWorkbench::CopyDefaultEffect(bool bAllSharing)
{
	const AZeroLockCharacter* CDO = Cast<AZeroLockCharacter>(GetHeroCDO());
	if (!CDO || !CDO->DefaultGameplayEffect)
	{
		return;
	}
	const UClass* Shared = CDO->DefaultGameplayEffect;
	TArray<FHeroPtr> Targets;
	for (const FHeroPtr& Hero : AllHeroes)
	{
		const AZeroLockCharacter* Other = Hero->GetHeroCDO();
		if (Other && Other->DefaultGameplayEffect == Shared && (bAllSharing || Hero == SelectedHero))
		{
			Targets.Add(Hero);
		}
	}
	TArray<FString> Done;
	for (const FHeroPtr& Hero : Targets)
	{
		FString Error;
		if (const UClass* NewEffect = ZLHeroAudit::CopyDefaultEffectForHero(*Hero, Error))
		{
			Done.Add(FString::Printf(TEXT("%s -> %s"), *Hero->Name, *NameText(NewEffect).ToString()));
			ZLHeroAudit::AuditHero(*Hero);
		}
		else
		{
			Done.Add(FString::Printf(TEXT("%s: %s"), *Hero->Name, *Error));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[HeroWorkbench] default effect copies: %s"), *FString::Join(Done, TEXT("; ")));
	SetDetailsObject(StatsDetails, GetDefaultEffectCDO());
	RefreshSelectedHero(false);
}

void SZL_HeroWorkbench::RefreshHeroes()
{
	const FString PreviousName = SelectedHero.IsValid() ? SelectedHero->Name : FString();
	AllHeroes = ZLHeroAudit::GatherHeroes(bShowUnlisted);
	ApplyHeroFilter();

	FHeroPtr Reselect;
	for (const FHeroPtr& Hero : FilteredHeroes)
	{
		if (Hero->Name == PreviousName)
		{
			Reselect = Hero;
		}
	}
	if (!Reselect && FilteredHeroes.Num() > 0 && !SelectedHero.IsValid())
	{
		Reselect = FilteredHeroes[0];
	}
	SelectedHero.Reset();
	if (Reselect)
	{
		HeroList->SetSelection(Reselect);
	}
	else
	{
		HeroList->ClearSelection();
		SelectHero(nullptr);
	}
}

void SZL_HeroWorkbench::ApplyHeroFilter()
{
	FilteredHeroes.Reset();
	for (const FHeroPtr& Hero : AllHeroes)
	{
		if (SearchText.IsEmpty() || Hero->Name.Contains(SearchText) || (Hero->Blueprint.IsValid() && Hero->Blueprint->GetName().Contains(SearchText)))
		{
			FilteredHeroes.Add(Hero);
		}
	}
	if (HeroList.IsValid())
	{
		HeroList->RequestListRefresh();
	}
}

void SZL_HeroWorkbench::SelectHero(FHeroPtr Hero)
{
	if (Hero == SelectedHero && Hero.IsValid())
	{
		return;
	}
	SelectedHero = Hero;
	RefreshSelectedHero(true);
}

void SZL_HeroWorkbench::RefreshSelectedHero(bool bResetDetails)
{
	const int32 SelectedSlotIndex = SlotList.IsValid() && SlotList->GetSelectedItems().Num() > 0 ? SlotItems.IndexOfByKey(SlotList->GetSelectedItems()[0]) : INDEX_NONE;

	SlotItems.Reset();
	HeroIssueItems.Reset();
	StatItems.Reset();

	if (SelectedHero.IsValid())
	{
		if (!bResetDetails)
		{
			ZLHeroAudit::AuditHero(*SelectedHero);
		}
		for (const FZL_AbilitySlot& Slot : SelectedHero->Slots)
		{
			SlotItems.Add(MakeShared<FZL_AbilitySlot>(Slot));
		}
		for (const FZL_AuditIssue& Issue : SelectedHero->Issues)
		{
			HeroIssueItems.Add(MakeShared<FZL_AuditIssue>(Issue));
		}
		HeroIssueItems.StableSort([](const FIssuePtr& A, const FIssuePtr& B) { return A->Severity > B->Severity; });

		if (const UGameplayEffect* Effect = Cast<UGameplayEffect>(GetDefaultEffectCDO()))
		{
			for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
			{
				TSharedPtr<FZL_StatRow> Row = MakeShared<FZL_StatRow>();
				Row->Attribute = Modifier.Attribute.GetName();
				Row->Operation = EGameplayModOpToString(Modifier.ModifierOp);
				float Value = 0.f;
				Row->Value = Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? FString::Printf(TEXT("%g"), Value) : FString(TEXT("(calculated)"));
				StatItems.Add(Row);
			}
		}
	}

	SetDetailsObject(DataDetails, SelectedHero.IsValid() ? SelectedHero->Data.Get() : nullptr);
	SetDetailsObject(SlotAssignDetails, GetHeroCDO());
	SetDetailsObject(HeroAnimDetails, GetHeroCDO());
	SetDetailsObject(MeshDetails, GetMeshComponent());
	SetDetailsObject(AnimSetDetails, GetAnimSet());
	SetDetailsObject(FireDetails, GetFireCDO());
	SetDetailsObject(ProjectileDetails, GetProjectileCDO());
	SetDetailsObject(ProjectileMoveDetails, GetProjectileMovement());
	SetDetailsObject(StatsDetails, GetDefaultEffectCDO());

	RefreshCurvePage();
	SlotList->RequestListRefresh();
	HeroIssueList->RequestListRefresh();
	StatList->RequestListRefresh();
	HeroList->RebuildList(); // issue badges

	if (SlotItems.IsValidIndex(SelectedSlotIndex) && !bResetDetails)
	{
		SlotList->SetSelection(SlotItems[SelectedSlotIndex], ESelectInfo::Direct);
	}
	else if (SlotItems.Num() > 0)
	{
		SlotList->SetSelection(SlotItems[0], ESelectInfo::Direct);
	}
	else
	{
		SetDetailsObject(AbilityDetails, nullptr);
	}
}

void SZL_HeroWorkbench::OnSlotSelected(FSlotPtr Slot, ESelectInfo::Type SelectInfo)
{
	SetDetailsObject(AbilityDetails, Slot.IsValid() && Slot->AbilityClass ? Slot->AbilityClass->GetDefaultObject() : nullptr);
}

void SZL_HeroWorkbench::RunProjectAudit()
{
	FScopedSlowTask SlowTask(1.f, LOCTEXT("Auditing", "Auditing heroes, abilities and content references..."));
	SlowTask.MakeDialogDelayed(0.5f);

	// The heroes the list shows: unlisted test characters only with "Show heroes not in the hero list".
	const TArray<FHeroPtr>& Heroes = AllHeroes;
	ProjectIssues.Reset();
	for (const FZL_AuditIssue& Issue : ZLHeroAudit::AuditProject(Heroes))
	{
		ProjectIssues.Add(MakeShared<FZL_AuditIssue>(Issue));
	}
	ProjectIssues.StableSort([](const FIssuePtr& A, const FIssuePtr& B) { return A->Severity > B->Severity; });
	bProjectAuditRun = true;

	int32 Counts[3] = { 0, 0, 0 };
	for (const FIssuePtr& Issue : ProjectIssues)
	{
		++Counts[static_cast<int32>(Issue->Severity)];
	}
	ProjectAuditSummary = FText::Format(LOCTEXT("AuditSummary", "{0} errors, {1} warnings, {2} info"), Counts[2], Counts[1], Counts[0]);
	RefreshProjectIssueItems();
}

void SZL_HeroWorkbench::RefreshProjectIssueItems()
{
	ProjectIssueItems.Reset();
	for (const FIssuePtr& Issue : ProjectIssues)
	{
		if (bShowInfoIssues || Issue->Severity != EZL_AuditSeverity::Info)
		{
			ProjectIssueItems.Add(Issue);
		}
	}
	if (ProjectIssueList.IsValid())
	{
		ProjectIssueList->RequestListRefresh();
	}
}

void SZL_HeroWorkbench::ExportCSV()
{
	if (!bProjectAuditRun)
	{
		RunProjectAudit();
	}
	TArray<FZL_AuditIssue> Issues;
	for (const FIssuePtr& Issue : ProjectIssues)
	{
		Issues.Add(*Issue);
	}
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ZeroLockAudit")));
	IFileManager::Get().MakeDirectory(*Dir, true);
	FString Error;
	if (ZLHeroAudit::ExportCSV(Dir, AllHeroes, Issues, Error))
	{
		UE_LOG(LogTemp, Log, TEXT("[HeroWorkbench] audit exported to %s"), *Dir);
		FPlatformProcess::ExploreFolder(*Dir);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[HeroWorkbench] %s"), *Error);
		ProjectAuditSummary = FText::FromString(Error);
	}
}

void SZL_HeroWorkbench::OnDetailsChanged(const FPropertyChangedEvent& Event, TWeakPtr<IDetailsView> View)
{
	const TSharedPtr<IDetailsView> Pinned = View.Pin();
	if (!Pinned)
	{
		return;
	}
	for (const TWeakObjectPtr<UObject>& Object : Pinned->GetSelectedObjects())
	{
		if (UObject* Edited = Object.Get())
		{
			Edited->MarkPackageDirty();
			if (UBlueprint* BP = Cast<UBlueprint>(EditableAsset(Edited)))
			{
				FBlueprintEditorUtils::PostEditChangeBlueprintActors(BP);
			}
		}
	}
	// Slot assignments, mesh or fire ability may have changed what the other panels show.
	const bool bStructural = Pinned == SlotAssignDetails || Pinned == HeroAnimDetails || Pinned == MeshDetails || Pinned == FireDetails;
	if (bStructural)
	{
		SetDetailsObject(AnimSetDetails, GetAnimSet());
		SetDetailsObject(ProjectileDetails, GetProjectileCDO());
		SetDetailsObject(ProjectileMoveDetails, GetProjectileMovement());
	}
	RefreshSelectedHero(false);
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

UObject* SZL_HeroWorkbench::GetHeroCDO() const
{
	return SelectedHero.IsValid() ? SelectedHero->GetHeroCDO() : nullptr;
}

UObject* SZL_HeroWorkbench::GetMeshComponent() const
{
	const UObject* CDO = GetHeroCDO();
	return CDO ? ZLHeroAudit::GetObjectProperty(CDO, TEXT("MeshComponent")) : nullptr;
}

UObject* SZL_HeroWorkbench::GetAnimSet() const
{
	return ZLHeroAudit::GetObjectProperty(GetHeroCDO(), TEXT("DeadlockAnimSet"));
}

UObject* SZL_HeroWorkbench::GetFireCDO() const
{
	const AZeroLockCharacter* CDO = Cast<AZeroLockCharacter>(GetHeroCDO());
	return CDO && CDO->PrimaryFireAbility ? CDO->PrimaryFireAbility->GetDefaultObject() : nullptr;
}

UObject* SZL_HeroWorkbench::GetProjectileCDO() const
{
	const UClass* Projectile = ZLHeroAudit::GetClassProperty(GetFireCDO(), TEXT("ProjectileClass"));
	return Projectile ? Projectile->GetDefaultObject() : nullptr;
}

UObject* SZL_HeroWorkbench::GetProjectileMovement() const
{
	return ZLHeroAudit::GetObjectProperty(GetProjectileCDO(), TEXT("ProjectileMovement"));
}

UObject* SZL_HeroWorkbench::GetDefaultEffectCDO() const
{
	const AZeroLockCharacter* CDO = Cast<AZeroLockCharacter>(GetHeroCDO());
	return CDO && CDO->DefaultGameplayEffect ? CDO->DefaultGameplayEffect->GetDefaultObject() : nullptr;
}

FText SZL_HeroWorkbench::GetWeaponSummary() const
{
	const UObject* Fire = GetFireCDO();
	if (!Fire)
	{
		return LOCTEXT("NoWeapon", "No primary fire ability");
	}
	TArray<FString> Parts;
	Parts.Add(FString::Printf(TEXT("Fire: %s"), *NameText(Fire->GetClass()).ToString()));
	if (const FProperty* FireMode = FindFProperty<FProperty>(Fire->GetClass(), TEXT("FireMode")))
	{
		FString Mode;
		FireMode->ExportText_InContainer(0, Mode, Fire, nullptr, nullptr, PPF_None);
		Parts.Add(FString::Printf(TEXT("mode %s"), *Mode));
	}
	if (const UObject* Projectile = GetProjectileCDO())
	{
		float Damage = 0.f;
		bool bDamage = false;
		ZLHeroAudit::GetFloatProperty(Projectile, TEXT("WeaponDamage"), Damage);
		ZLHeroAudit::GetBoolProperty(Projectile, TEXT("bApplyWeaponDamage"), bDamage);
		Parts.Add(FString::Printf(TEXT("projectile %s, damage %s"), *NameText(Projectile->GetClass()).ToString(), bDamage ? *FString::Printf(TEXT("%g"), Damage) : TEXT("off")));
		float Speed = 0.f;
		if (ZLHeroAudit::GetFloatProperty(GetProjectileMovement(), TEXT("InitialSpeed"), Speed))
		{
			Parts.Add(FString::Printf(TEXT("speed %g"), Speed));
		}
	}
	// Fire rate and ammo are attributes, set by the hero's DefaultGameplayEffect.
	for (const FStatPtr& Stat : StatItems)
	{
		if (Stat->Attribute.Contains(TEXT("FireRate")) || Stat->Attribute.Contains(TEXT("Ammo")))
		{
			Parts.Add(FString::Printf(TEXT("%s %s"), *Stat->Attribute, *Stat->Value));
		}
	}
	return FText::FromString(FString::Join(Parts, TEXT("   |   ")));
}

const FSlateBrush* SZL_HeroWorkbench::GetBrush(UTexture2D* Texture, float Size)
{
	if (!Texture)
	{
		return nullptr;
	}
	TSharedPtr<FSlateBrush>& Brush = Brushes.FindOrAdd(FObjectKey(Texture));
	if (!Brush)
	{
		Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Texture);
		Brush->ImageSize = FVector2D(Size, Size);
	}
	return Brush.Get();
}

void SZL_HeroWorkbench::SetDetailsObject(const TSharedPtr<IDetailsView>& View, UObject* Object)
{
	if (!View.IsValid())
	{
		return;
	}
	const TArray<TWeakObjectPtr<UObject>>& Current = View->GetSelectedObjects();
	if (Current.Num() == 1 && Current[0].Get() == Object)
	{
		return;
	}
	View->SetObject(Object, true);
}

void SZL_HeroWorkbench::OpenAsset(UObject* Asset)
{
	if (Asset && GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
	}
}

void SZL_HeroWorkbench::BrowseToAsset(UObject* Asset)
{
	if (Asset && GEditor)
	{
		TArray<UObject*> Objects = { Asset };
		GEditor->SyncBrowserToObjects(Objects);
	}
}

#undef LOCTEXT_NAMESPACE
