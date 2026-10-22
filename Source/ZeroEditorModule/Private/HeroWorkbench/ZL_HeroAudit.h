// Copyright Preetham Mukundan (C) 2026

#pragma once

#include "CoreMinimal.h"

class AZeroLockCharacter;
class UBaseGameplayAbility;
class UBlueprint;
class UTexture2D;
class UZL_Character_Data_Asset;
class UCurveTable;

/** One FScalableFloat on one of a hero's own abilities (cooldown, damage, radius...), and where its value comes from. */
struct FZL_CurveLink
{
	TSubclassOf<UBaseGameplayAbility> AbilityClass;
	FName Property;
	/** "CallBell" for BP_Doorman_CallBell: the row name prefix. */
	FString AbilityShortName;
	/** Null: a constant (Value). */
	TWeakObjectPtr<UCurveTable> Table;
	FName Row;
	float Value = 0.f;
	/** Table set but the row isn't in it: the value silently becomes 0. */
	bool bRowMissing = false;
};

enum class EZL_AuditSeverity : uint8
{
	Info,
	Warning,
	Error,
};

struct FZL_AuditIssue
{
	EZL_AuditSeverity Severity = EZL_AuditSeverity::Warning;
	/** Hero display name, or empty for project-wide issues. */
	FString Hero;
	/** What the issue is about: a slot, "Mesh", "Weapon", an asset name. */
	FString Context;
	FString Message;
	/** Asset to open from the issue (Blueprint, data asset, montage...). */
	FSoftObjectPath Asset;
};

/** One ability property on the hero class (Ability_1, UltimateAbility, DefaultAbilities[2]...). */
struct FZL_AbilitySlot
{
	FName PropertyName;
	FString Label;
	TSubclassOf<UBaseGameplayAbility> AbilityClass;
	/** Active slot that should have a cooldown (secondary, abilities 1/2, ultimate). */
	bool bExpectsCooldown = false;
	/** An element of an array property (DefaultAbilities): listed after the single slots. */
	bool bFromArray = false;
	TArray<FZL_AuditIssue> Issues;
};

struct FZL_HeroEntry
{
	FString Name;
	TWeakObjectPtr<UZL_Character_Data_Asset> Data;
	TSubclassOf<AZeroLockCharacter> HeroClass;
	TWeakObjectPtr<UBlueprint> Blueprint;
	/** In the game instance's hero table (hero select); false for other hero Blueprints. */
	bool bInHeroList = false;
	FLinearColor Color = FLinearColor::Gray;
	TWeakObjectPtr<UTexture2D> Icon;

	TArray<FZL_AbilitySlot> Slots;
	/** Every issue of this hero, slot issues included. */
	TArray<FZL_AuditIssue> Issues;

	int32 CountIssues(EZL_AuditSeverity Severity) const;
	AZeroLockCharacter* GetHeroCDO() const;
};

/** Editor-only data gathering and checks for the hero workbench (SZL_HeroWorkbench) and the CSV export. */
namespace ZLHeroAudit
{
	/** Heroes from the game instance's CharacterDataTable (in table order), then, if bIncludeUnlisted, every other
	 * AZeroLockCharacter Blueprint. Each entry is already audited. */
	TArray<TSharedPtr<FZL_HeroEntry>> GatherHeroes(bool bIncludeUnlisted);

	void AuditHero(FZL_HeroEntry& Hero);

	/** Checks on one ability class (icon, name, cooldown setup, debug flags left on). */
	void AuditAbility(UClass* AbilityClass, const FString& Hero, const FString& SlotLabel, bool bExpectsCooldown, TArray<FZL_AuditIssue>& Out);

	/** Every UBaseGameplayAbility Blueprint under /Game (loads them). */
	TArray<UClass*> GetAllAbilityBlueprintClasses();

	/** Hero issues + unused ability Blueprints + shipping content referencing Developers/Deadlock assets. */
	TArray<FZL_AuditIssue> AuditProject(const TArray<TSharedPtr<FZL_HeroEntry>>& Heroes);

	/** Writes AbilityAudit.csv, HeroSlots.csv and Issues.csv into Dir. */
	bool ExportCSV(const FString& Dir, const TArray<TSharedPtr<FZL_HeroEntry>>& Heroes, const TArray<FZL_AuditIssue>& Issues, FString& OutError);

	/** "Name=Value; ..." of the editable numbers/bools/enums declared below StopAtClass (the per-ability tuning values). */
	FString DescribeTunables(const UObject* Object, const UClass* StopAtClass);

	/** Cooldown seconds at level 1 (the first upgrade), or 0. */
	float GetCooldown(const UBaseGameplayAbility* Ability);

	UBlueprint* GetBlueprint(const UClass* Class);
	FString SeverityToString(EZL_AuditSeverity Severity);
	FLinearColor SeverityColor(EZL_AuditSeverity Severity);

	/** Reflection helpers: read a property by name, so protected members and Blueprint variables work too. */
	UObject* GetObjectProperty(const UObject* Object, FName Name);
	UClass* GetClassProperty(const UObject* Object, FName Name);
	bool GetFloatProperty(const UObject* Object, FName Name, float& OutValue);
	bool GetBoolProperty(const UObject* Object, FName Name, bool& OutValue);

	/** "Doorman" for "The Doorman": asset name part. */
	FString GetHeroShortName(const FZL_HeroEntry& Hero);
	/** The /Game/ZERO/HeroAbilities/<X> folder most of the hero's abilities live in, else one named after the hero. */
	FString GetHeroContentFolder(const FZL_HeroEntry& Hero);

	/** Every FScalableFloat on the hero's own ability Blueprints (those under /Game/ZERO/HeroAbilities/). */
	TArray<FZL_CurveLink> GetHeroScalableFloats(const FZL_HeroEntry& Hero);
	/** The curve table most of the hero's ability values use, or null. */
	UCurveTable* FindHeroCurveTable(const FZL_HeroEntry& Hero);
	/** C_<Hero> in the hero's content folder (loads it if it already exists). */
	UCurveTable* CreateHeroCurveTable(const FZL_HeroEntry& Hero, FString& OutError);
	/** Moves every constant (non-zero) ability value into Table: row <Ability>_<Property> keyed 0..NumLevels-1 with the
	 * current value, the property becomes Value 1 x that row. Existing rows are linked, not overwritten. Returns the count. */
	int32 LinkHeroValuesToTable(const FZL_HeroEntry& Hero, UCurveTable* Table, int32 NumLevels = 4);

	bool AddCurveTableRow(UCurveTable* Table, FName Row, float Value, int32 NumLevels);
	void RemoveCurveTableRow(UCurveTable* Table, FName Row);
	void SetCurveTableValue(UCurveTable* Table, FName Row, float Level, float Value);
	float GetCurveTableValue(const UCurveTable* Table, FName Row, float Level);

	/** Duplicates the hero's DefaultGameplayEffect Blueprint to <hero folder>/GE_<Hero>_DefaultStats (or reuses that asset)
	 * and assigns it to the hero. Returns the new effect class. */
	UClass* CopyDefaultEffectForHero(const FZL_HeroEntry& Hero, FString& OutError);
	/** Marks a Blueprint CDO (or its subobject) edited: dirty package, placed instances updated. */
	void MarkClassDefaultsEdited(UObject* Object);
}
