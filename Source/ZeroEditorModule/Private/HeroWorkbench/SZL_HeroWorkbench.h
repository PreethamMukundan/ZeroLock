// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "HeroWorkbench/ZL_HeroAudit.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class UCurveTable;
class FProperty;
struct FPropertyChangedEvent;
struct FSlateBrush;

enum class EZL_WorkbenchPage : uint8
{
	Overview,
	Abilities,
	Mesh,
	Weapon,
	Stats,
	Curves,
	Audit,
};

/** One row of the hero's curve table, with the ability values that use it. */
struct FZL_CurveRow
{
	FName Row;
	FString UsedBy;
};

/** One modifier of the hero's DefaultGameplayEffect (its base stats). */
struct FZL_StatRow
{
	FString Attribute;
	FString Operation;
	FString Value;
};

/**
 * ZeroLock > Hero Workbench. Left: the heroes (hero table first, other hero Blueprints optional). Right: pages for the
 * selected hero (Overview, Abilities, Mesh, Weapon, Stats, Curve Table) and the project-wide Audit with CSV export.
 * The details panels edit the Blueprints' class defaults directly: the package is marked dirty, Save writes it.
 */
class SZL_HeroWorkbench : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SZL_HeroWorkbench) {}
	SLATE_END_ARGS()

	static const FName TabName;

	void Construct(const FArguments& InArgs);

private:
	using FHeroPtr = TSharedPtr<FZL_HeroEntry>;
	using FSlotPtr = TSharedPtr<FZL_AbilitySlot>;
	using FIssuePtr = TSharedPtr<FZL_AuditIssue>;
	using FStatPtr = TSharedPtr<FZL_StatRow>;
	using FCurveRowPtr = TSharedPtr<FZL_CurveRow>;
	using FCurveLinkPtr = TSharedPtr<FZL_CurveLink>;

	// Data
	TArray<FHeroPtr> AllHeroes;
	TArray<FHeroPtr> FilteredHeroes;
	FHeroPtr SelectedHero;
	TArray<FSlotPtr> SlotItems;
	TArray<FIssuePtr> HeroIssueItems;
	TArray<FIssuePtr> ProjectIssues;
	TArray<FIssuePtr> ProjectIssueItems;
	TArray<FStatPtr> StatItems;
	TWeakObjectPtr<UCurveTable> CurveTable;
	TArray<FCurveRowPtr> CurveRows;
	TArray<FCurveLinkPtr> CurveLinks;
	FString NewCurveRowName;
	float NewCurveRowValue = 0.f;
	FText CurveStatus;
	TMap<FObjectKey, TSharedPtr<FSlateBrush>> Brushes;
	EZL_WorkbenchPage Page = EZL_WorkbenchPage::Overview;
	FString SearchText;
	bool bShowUnlisted = false;
	bool bShowInfoIssues = true;
	bool bProjectAuditRun = false;
	FText ProjectAuditSummary;

	// Widgets
	TSharedPtr<SListView<FHeroPtr>> HeroList;
	TSharedPtr<SListView<FSlotPtr>> SlotList;
	TSharedPtr<SListView<FIssuePtr>> HeroIssueList;
	TSharedPtr<SListView<FIssuePtr>> ProjectIssueList;
	TSharedPtr<SListView<FStatPtr>> StatList;
	TSharedPtr<IDetailsView> DataDetails;
	TSharedPtr<IDetailsView> SlotAssignDetails;
	TSharedPtr<IDetailsView> AbilityDetails;
	TSharedPtr<IDetailsView> HeroAnimDetails;
	TSharedPtr<IDetailsView> MeshDetails;
	TSharedPtr<IDetailsView> AnimSetDetails;
	TSharedPtr<IDetailsView> FireDetails;
	TSharedPtr<IDetailsView> ProjectileDetails;
	TSharedPtr<IDetailsView> ProjectileMoveDetails;
	TSharedPtr<IDetailsView> StatsDetails;
	TSharedPtr<SListView<FCurveRowPtr>> CurveRowList;
	TSharedPtr<SListView<FCurveLinkPtr>> CurveLinkList;

	// Layout
	TSharedRef<SWidget> BuildHeroPanel();
	TSharedRef<SWidget> BuildHeroHeader();
	TSharedRef<SWidget> BuildOverviewPage();
	TSharedRef<SWidget> BuildAbilitiesPage();
	TSharedRef<SWidget> BuildMeshPage();
	TSharedRef<SWidget> BuildWeaponPage();
	TSharedRef<SWidget> BuildStatsPage();
	TSharedRef<SWidget> BuildCurvesPage();
	TSharedRef<SWidget> BuildAuditPage();
	TSharedRef<SWidget> BuildIssueList(TSharedPtr<SListView<FIssuePtr>>& OutList, TArray<FIssuePtr>* Items, bool bShowHeroColumn);
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Content);
	TSharedRef<SWidget> MakeAssetButton(const FText& Label, TFunction<UObject*()> GetAsset);
	TSharedRef<IDetailsView> MakeDetails(TFunction<bool(const FProperty&)> Filter = nullptr);

	TSharedRef<ITableRow> MakeHeroRow(FHeroPtr Hero, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<SWidget> MakeSlotColumn(const FName& Column, const FSlotPtr& Slot);
	TSharedRef<SWidget> MakeIssueColumn(const FName& Column, const FIssuePtr& Issue);
	TSharedRef<SWidget> MakeStatColumn(const FName& Column, const FStatPtr& Stat);
	TSharedRef<SWidget> MakeCurveRowColumn(const FName& Column, const FCurveRowPtr& Row);
	TSharedRef<SWidget> MakeCurveLinkColumn(const FName& Column, const FCurveLinkPtr& Link);

	// Behaviour
	void RefreshHeroes();
	void ApplyHeroFilter();
	void SelectHero(FHeroPtr Hero);
	/** Re-audits the selected hero and rebuilds its lists; details panels only change object when it changed. */
	void RefreshSelectedHero(bool bResetDetails);
	void RunProjectAudit();
	void RefreshProjectIssueItems();
	void ExportCSV();
	void OnDetailsChanged(const FPropertyChangedEvent& Event, TWeakPtr<IDetailsView> View);
	void OnSlotSelected(FSlotPtr Slot, ESelectInfo::Type SelectInfo);

	UObject* GetHeroCDO() const;
	UObject* GetMeshComponent() const;
	UObject* GetAnimSet() const;
	UObject* GetFireCDO() const;
	UObject* GetProjectileCDO() const;
	UObject* GetProjectileMovement() const;
	UObject* GetDefaultEffectCDO() const;
	FText GetWeaponSummary() const;
	/** Other heroes (names) using the selected hero's DefaultGameplayEffect. */
	TArray<FString> GetHeroesSharingDefaultEffect() const;
	void CopyDefaultEffect(bool bAllSharing);
	void RefreshCurvePage();
	void CreateCurveTable();
	void LinkValuesToCurveTable();
	void AddCurveRow();
	void DeleteCurveRow(FName Row);

	const FSlateBrush* GetBrush(UTexture2D* Texture, float Size);
	static void SetDetailsObject(const TSharedPtr<IDetailsView>& View, UObject* Object);
	static void OpenAsset(UObject* Asset);
	static void BrowseToAsset(UObject* Asset);
};
