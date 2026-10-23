// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FZL_DeadlockHeroInfo : public TSharedFromThis<FZL_DeadlockHeroInfo>
{
public:
	FString DisplayName;
	FString Codename;
	FString ModelPath;
	bool bIsImported = false;
};

class SZL_DeadlockImporter : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SZL_DeadlockImporter) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static const FName TabName;

private:
	TArray<TSharedPtr<FZL_DeadlockHeroInfo>> AllHeroes;
	TArray<TSharedPtr<FZL_DeadlockHeroInfo>> FilteredHeroes;
	TSharedPtr<SListView<TSharedPtr<FZL_DeadlockHeroInfo>>> HeroListView;

	FString StatusMessage = TEXT("Ready.");
	bool bIncludeAnimations = true;
	bool bIsImporting = false;

	void ScanHeroes();
	void OnSearchTextChanged(const FText& Text);
	TSharedRef<ITableRow> OnGenerateHeroRow(TSharedPtr<FZL_DeadlockHeroInfo> Hero, const TSharedRef<STableViewBase>& OwnerTable);

	FReply OnRunImportClicked(TSharedPtr<FZL_DeadlockHeroInfo> Hero);
	FString GetToolDirectory() const;
};