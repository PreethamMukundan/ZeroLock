// Copyright Preetham Mukundan (C) 2026


#include "ZL_DeadlockImporter.h"
#include "Async/Async.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "SZL_DeadlockImporter"

const FName SZL_DeadlockImporter::TabName(TEXT("ZeroLockDeadlockImporter"));

namespace
{
	const FName ColHeroName(TEXT("HeroName"));
	const FName ColCodename(TEXT("Codename"));
	const FName ColStatus(TEXT("Status"));
	const FName ColAction(TEXT("Action"));
}

void SZL_DeadlockImporter::Construct(const FArguments& InArgs)
{
	ScanHeroes();

	ChildSlot
	[
		SNew(SVerticalBox)
		// Top toolbar / Search & Options
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Search Deadlock heroes by name or codename..."))
				.OnTextChanged(this, &SZL_DeadlockImporter::OnSearchTextChanged)
				.IsEnabled_Lambda([this]() { return !bIsImporting; })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Rescan", "Rescan Local/Project"))
				.IsEnabled_Lambda([this]() { return !bIsImporting; })
				.OnClicked_Lambda([this]()
				{
					ScanHeroes();
					return FReply::Handled();
				})
			]
		]
		// Settings bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 0.f, 8.f, 8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bIncludeAnimations ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bIncludeAnimations = (NewState == ECheckBoxState::Checked); })
				.IsEnabled_Lambda([this]() { return !bIsImporting; })
				[
					SNew(STextBlock).Text(LOCTEXT("IncludeAnims", "Include All Animations"))
				]
			]
		]
		// Main Hero List Table (Disabled during import)
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(8.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(HeroListView, SListView<TSharedPtr<FZL_DeadlockHeroInfo>>)
				.ListItemsSource(&FilteredHeroes)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SZL_DeadlockImporter::OnGenerateHeroRow)
				.IsEnabled_Lambda([this]() { return !bIsImporting; })
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(ColHeroName).DefaultLabel(LOCTEXT("ColHeroName", "Display Name")).FillWidth(0.3f)
					+ SHeaderRow::Column(ColCodename).DefaultLabel(LOCTEXT("ColCodename", "Codename")).FillWidth(0.3f)
					+ SHeaderRow::Column(ColStatus).DefaultLabel(LOCTEXT("ColStatus", "Import Status")).FillWidth(0.2f)
					+ SHeaderRow::Column(ColAction).DefaultLabel(LOCTEXT("ColAction", "Actions")).FillWidth(0.2f)
				)
			]
		]
		// Status bar at the bottom with an active loading indicator
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SImage)
				.Visibility_Lambda([this]() { return bIsImporting ? EVisibility::Visible : EVisibility::Collapsed; })
				.Image(FAppStyle::GetBrush("Icons.Loading"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(StatusMessage); })
				.ColorAndOpacity_Lambda([this]() 
				{ 
					return bIsImporting ? FSlateColor(FLinearColor::Yellow) : FSlateColor::UseSubduedForeground(); 
				})
			]
		]
	];
}

FString SZL_DeadlockImporter::GetToolDirectory() const
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/DeadlockImporter/"));
}

void SZL_DeadlockImporter::ScanHeroes()
{
	AllHeroes.Empty();

	FString ToolDir = GetToolDirectory();
	FString BatPath = FPaths::Combine(ToolDir, TEXT("deadlock_import.bat"));
	FString TempOutPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp/deadlock_heroes.txt"));
	IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp")), true);

	if (FPaths::FileExists(BatPath))
	{
		FString Args = TEXT("--list > ") + TempOutPath;
		int32 ReturnCode = 0;
		FPlatformProcess::ExecProcess(*BatPath, *Args, &ReturnCode, nullptr, nullptr, *ToolDir);

		FString FileContent;
		if (FFileHelper::LoadFileToString(FileContent, *TempOutPath))
		{
			TArray<FString> Lines;
			FileContent.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				FString Trimmed = Line.TrimStartAndEnd();
				if (Trimmed.IsEmpty())
				{
					continue;
				}

				// The line might contain multiple tokens/columns separated by whitespace or tabs (e.g. "Viscous viscous models/heroes...")
				TArray<FString> Tokens;
				Trimmed.ParseIntoArrayWS(Tokens);

				if (Tokens.Num() > 0)
				{
					TSharedPtr<FZL_DeadlockHeroInfo> HeroInfo = MakeShared<FZL_DeadlockHeroInfo>();
					
					// First token is Display Name, second (if exists) is Codename
					HeroInfo->DisplayName = Tokens[0];
					HeroInfo->Codename = (Tokens.Num() > 1) ? Tokens[1] : Tokens[0];

					FString AssetFolder = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Developers/rpm86/Deadlock"), HeroInfo->DisplayName);
					HeroInfo->bIsImported = IFileManager::Get().DirectoryExists(*AssetFolder);

					AllHeroes.Add(HeroInfo);
				}
			}
		}
	}

	// Fallback/Sample entries if list file was empty or script failed to output
	if (AllHeroes.Num() == 0)
	{
		TSharedPtr<FZL_DeadlockHeroInfo> Lash = MakeShared<FZL_DeadlockHeroInfo>();
		Lash->DisplayName = TEXT("Lash");
		Lash->Codename = TEXT("lash");
		Lash->bIsImported = IFileManager::Get().DirectoryExists(*(FPaths::ProjectContentDir() / TEXT("Developers/rpm86/Deadlock/Lash")));
		AllHeroes.Add(Lash);

		TSharedPtr<FZL_DeadlockHeroInfo> Apollo = MakeShared<FZL_DeadlockHeroInfo>();
		Apollo->DisplayName = TEXT("Apollo");
		Apollo->Codename = TEXT("apollo");
		Apollo->bIsImported = IFileManager::Get().DirectoryExists(*(FPaths::ProjectContentDir() / TEXT("Developers/rpm86/Deadlock/Apollo")));
		AllHeroes.Add(Apollo);
	}

	FilteredHeroes = AllHeroes;
	if (HeroListView.IsValid())
	{
		HeroListView->RequestListRefresh();
	}
	StatusMessage = FString::Printf(TEXT("Scanned %d heroes from Deadlock importer tools."), AllHeroes.Num());
}

void SZL_DeadlockImporter::OnSearchTextChanged(const FText& Text)
{
	FString FilterStr = Text.ToString();
	FilteredHeroes.Empty();
	for (const TSharedPtr<FZL_DeadlockHeroInfo>& Hero : AllHeroes)
	{
		if (FilterStr.IsEmpty() || Hero->DisplayName.Contains(FilterStr, ESearchCase::IgnoreCase) || Hero->Codename.Contains(FilterStr, ESearchCase::IgnoreCase))
		{
			FilteredHeroes.Add(Hero);
		}
	}
	if (HeroListView.IsValid())
	{
		HeroListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SZL_DeadlockImporter::OnGenerateHeroRow(TSharedPtr<FZL_DeadlockHeroInfo> Hero, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FZL_DeadlockHeroInfo>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.3f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Hero->DisplayName))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.3f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Hero->Codename))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.2f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Hero->bIsImported ? LOCTEXT("Imported", "Imported") : LOCTEXT("NotImported", "Not Imported"))
				.ColorAndOpacity(Hero->bIsImported ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Gray))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.2f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("ImportButton", "Run Import"))
				.IsEnabled_Lambda([this]() { return !bIsImporting; })
				.OnClicked_Lambda([this, Hero]()
				{
					return OnRunImportClicked(Hero);
				})
			]
		];
}

FReply SZL_DeadlockImporter::OnRunImportClicked(TSharedPtr<FZL_DeadlockHeroInfo> Hero)
{
	if (!Hero.IsValid() || bIsImporting)
	{
		return FReply::Handled();
	}

	bIsImporting = true;
	StatusMessage = FString::Printf(TEXT("[BUSY] Stopping old tasks & running import for %s..."), *Hero->DisplayName);
	UE_LOG(LogTemp, Log, TEXT("[DeadlockImporter] Initiating pipeline for hero: %s"), *Hero->DisplayName);

	Async(EAsyncExecution::ThreadPool, [this, Hero]()
	{
		// Force-kill any hanging background tasks or helper tools from previous runs to clear the slate
		int32 DummyCode = 0;
		FPlatformProcess::ExecProcess(TEXT("taskkill"), TEXT("/f /im blender.exe"), &DummyCode, nullptr, nullptr);
		FPlatformProcess::ExecProcess(TEXT("taskkill"), TEXT("/f /im python.exe"), &DummyCode, nullptr, nullptr);

		FString ToolDir = GetToolDirectory();
		FString BatPath = FPaths::Combine(ToolDir, TEXT("deadlock_import.bat"));
		FString PyScript = FPaths::Combine(ToolDir, TEXT("ue_import.py"));

		// 1. Run Export Step
		FString ExportArgs = FString::Printf(TEXT("%s"), *Hero->Codename);
		if (!bIncludeAnimations)
		{
			ExportArgs += TEXT(" --anims none");
		}

		int32 OutReturnCode = -1;
		FString StdOut, StdErr;

		bool bExportSuccess = FPlatformProcess::ExecProcess(
			*BatPath, *ExportArgs, &OutReturnCode, &StdOut, &StdErr, *ToolDir
		);

		if (!StdOut.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("[DeadlockImporter Export Output]:\n%s"), *StdOut);
		}
		if (!StdErr.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[DeadlockImporter Export Error]:\n%s"), *StdErr);
		}

		// 2. Run Python Unreal Import Step if export succeeded
		if (bExportSuccess && OutReturnCode == 0)
		{
		  
		   FPaths::NormalizeFilename(PyScript);

		   // Format as: py "E:/Repo/.../ue_import.py" Solomon
		   FString PyCommand = FString::Printf(TEXT("py \"%s\" %s"), *PyScript, *Hero->DisplayName);

		   UE_LOG(LogTemp, Log, TEXT("[DeadlockImporter] Executing console command: %s"), *PyCommand);

		   // Get world context from GEngine
		   UWorld* WorldContext = nullptr;
		   if (GEngine && GEngine->GameViewport)
		   {
			  WorldContext = GEngine->GameViewport->GetWorld();
		   }

		   // Execute the python console command
		   UKismetSystemLibrary::ExecuteConsoleCommand(WorldContext, PyCommand, nullptr);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[DeadlockImporter] Export step failed. Skipping import."));
		}

		// Finish and reset state back on the Game Thread
		AsyncTask(ENamedThreads::GameThread, [this, Hero]()
		{
			bIsImporting = false;
			StatusMessage = FString::Printf(TEXT("Finished import pipeline for %s! Check Output Log."), *Hero->DisplayName);
			ScanHeroes();
		});
	});

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE