// Copyright Preetham Mukundan (C) 2026

// Console versions of the Hero Workbench actions (scriptable, e.g. from the Unreal MCP). They mark assets dirty; save
// them as usual (File > Save All, or the workbench's Save...).
//   ZL.Workbench.CopyDefaultEffect <Hero|all>   per-hero copy of the DefaultGameplayEffect (GE_<Hero>_DefaultStats)
//   ZL.Workbench.CurveTable <Hero> [link]       find or create C_<Hero>; "link" moves the constant ability values into it

#include "Engine/CurveTable.h"
#include "HAL/IConsoleManager.h"
#include "HeroWorkbench/ZL_HeroAudit.h"

namespace
{
	TArray<TSharedPtr<FZL_HeroEntry>> FindHeroes(const FString& Name)
	{
		TArray<TSharedPtr<FZL_HeroEntry>> Found;
		for (const TSharedPtr<FZL_HeroEntry>& Hero : ZLHeroAudit::GatherHeroes(false))
		{
			const bool bMatch = Name.Equals(TEXT("all"), ESearchCase::IgnoreCase) || Hero->Name.Equals(Name, ESearchCase::IgnoreCase)
				|| ZLHeroAudit::GetHeroShortName(*Hero).Equals(Name, ESearchCase::IgnoreCase);
			if (bMatch)
			{
				Found.Add(Hero);
			}
		}
		if (Found.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HeroWorkbench] no hero in the hero list matches '%s'"), *Name);
		}
		return Found;
	}

	FAutoConsoleCommand CopyDefaultEffectCommand(
		TEXT("ZL.Workbench.CopyDefaultEffect"),
		TEXT("ZL.Workbench.CopyDefaultEffect <Hero|all>: give the hero its own copy of its DefaultGameplayEffect (<hero ability folder>/GE_<Hero>_DefaultStats)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			for (const TSharedPtr<FZL_HeroEntry>& Hero : FindHeroes(Args.Num() > 0 ? Args[0] : FString(TEXT("all"))))
			{
				FString Error;
				const UClass* Effect = ZLHeroAudit::CopyDefaultEffectForHero(*Hero, Error);
				UE_LOG(LogTemp, Log, TEXT("[HeroWorkbench] %s: %s"), *Hero->Name, Effect ? *Effect->GetPathName() : *Error);
			}
		}));

	FAutoConsoleCommand CurveTableCommand(
		TEXT("ZL.Workbench.CurveTable"),
		TEXT("ZL.Workbench.CurveTable <Hero> [link]: find or create the hero's curve table (C_<Hero>); with 'link', move its constant ability values into it."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[HeroWorkbench] usage: ZL.Workbench.CurveTable <Hero> [link]"));
				return;
			}
			const bool bLink = Args.Num() > 1 && Args[1].Equals(TEXT("link"), ESearchCase::IgnoreCase);
			for (const TSharedPtr<FZL_HeroEntry>& Hero : FindHeroes(Args[0]))
			{
				FString Error;
				UCurveTable* Table = ZLHeroAudit::FindHeroCurveTable(*Hero);
				if (!Table)
				{
					Table = ZLHeroAudit::CreateHeroCurveTable(*Hero, Error);
				}
				if (!Table)
				{
					UE_LOG(LogTemp, Warning, TEXT("[HeroWorkbench] %s: %s"), *Hero->Name, *Error);
					continue;
				}
				const int32 Linked = bLink ? ZLHeroAudit::LinkHeroValuesToTable(*Hero, Table) : 0;
				TArray<FName> Rows;
				Table->GetRowMap().GetKeys(Rows);
				TArray<FString> RowStrings;
				for (const FName& Row : Rows)
				{
					RowStrings.Add(FString::Printf(TEXT("%s=%g/%g/%g/%g"), *Row.ToString(),
						ZLHeroAudit::GetCurveTableValue(Table, Row, 0.f), ZLHeroAudit::GetCurveTableValue(Table, Row, 1.f),
						ZLHeroAudit::GetCurveTableValue(Table, Row, 2.f), ZLHeroAudit::GetCurveTableValue(Table, Row, 3.f)));
				}
				UE_LOG(LogTemp, Log, TEXT("[HeroWorkbench] %s: %s, linked %d, rows: %s"), *Hero->Name, *Table->GetPathName(), Linked, *FString::Join(RowStrings, TEXT(", ")));
			}
		}));
}
