#include "ZeroEditorModule.h"
#include "LevelEditor.h"
#include "DeadlockImporter/ZL_DeadlockImporter.h"
#include "Framework/Docking/TabManager.h"
#include "GAS/BaseGameplayAbility.h"
#include "HeroWorkbench/SZL_HeroWorkbench.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FZeroEditorModuleModule"

DEFINE_LOG_CATEGORY(LogZeroEditor);

void FZeroEditorModuleModule::StartupModule()
{
    //Getting the ref
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    //MEnu Extender - add menus to the bar
    MenuExtender = MakeShareable(new FExtender());

    //add menu bar to it
    MenuExtender->AddMenuBarExtension("Help",
        EExtensionHook::Before,
        nullptr,
        FMenuBarExtensionDelegate::CreateRaw(this,&FZeroEditorModuleModule::AddMenu));


	//adding it in
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SZL_HeroWorkbench::TabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SZL_HeroWorkbench)
				];
		}))
		.SetDisplayName(LOCTEXT("HeroWorkbenchTab", "Hero Workbench"))
		.SetTooltipText(LOCTEXT("HeroWorkbenchTabTip", "Heroes, their abilities, mesh, weapon and stats, and the project audit"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SZL_DeadlockImporter::TabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SZL_DeadlockImporter)
			];
	}))
	.SetDisplayName(LOCTEXT("DeadlockImporterTab", "Deadlock Hero Importer"))
	.SetTooltipText(LOCTEXT("DeadlockImporterTabTip", "Browse, export, and import Deadlock heroes and animations into Unreal"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FZeroEditorModuleModule::AddMenu(FMenuBarBuilder& MenuBarBuilder)
{
	MenuBarBuilder.AddPullDownMenu(FText::FromString("ZeroLock"),
		FText::FromString("ZeroLock tools"),
		FNewMenuDelegate::CreateRaw(this,&FZeroEditorModuleModule::FillMenu));
}

void FZeroEditorModuleModule::FillMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(LOCTEXT("HeroWorkbench", "Hero Workbench"),
		LOCTEXT("HeroWorkbenchTip", "Browse and edit every hero (abilities, mesh, weapon, base stats) and run the project audit"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SZL_HeroWorkbench::TabName); })));

	MenuBuilder.AddMenuEntry(FText::FromString("BaseAbilityList"),
		FText::FromString("Base Ability List"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this,&FZeroEditorModuleModule::PrintAllAbilities)));
	
	MenuBuilder.AddMenuEntry(LOCTEXT("DeadlockImporterMenu", "Deadlock Hero Importer"),
	LOCTEXT("DeadlockImporterMenuTip", "Open the Deadlock hero management and automated import panel"),
	FSlateIcon(),
	FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(SZL_DeadlockImporter::TabName); })));
}

void FZeroEditorModuleModule::PrintAllAbilities()
{
	TArray<UClass*> foundClasses;

		for (TObjectIterator<UClass> classIterator; classIterator; ++classIterator)
		{
			UClass* Class = *classIterator;

			if (Class && Class->IsChildOf(UBaseGameplayAbility::StaticClass()) && Class != UBaseGameplayAbility::StaticClass())
			{
				foundClasses.Add(Class);
			}
		}

		for (UClass* Class : foundClasses)
		{
			UE_LOG(LogTemp, Warning, TEXT("Found Class: %s"), *Class->GetName());
			// Iterate through properties using TFieldIterator
			for (TFieldIterator<FProperty> PropertyIt(Class); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;

				FString PropertyName = Property->GetName();
				FString PropertyTypeName = Property->GetClass()->GetName();

				UE_LOG(LogTemp, Log, TEXT("Property: %s, Type: %s"), *PropertyName, *PropertyTypeName);
			}
		}

}

void FZeroEditorModuleModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SZL_HeroWorkbench::TabName);
	}
	if (MenuExtender.IsValid() && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetMenuExtensibilityManager()->RemoveExtender(MenuExtender);
	}
	
	MenuExtender.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FZeroEditorModuleModule, ZeroEditorModule)
