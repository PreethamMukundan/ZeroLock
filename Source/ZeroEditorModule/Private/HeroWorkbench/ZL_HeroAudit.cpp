// Copyright Preetham Mukundan (C) 2026

#include "HeroWorkbench/ZL_HeroAudit.h"

#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CurveTableEditorUtils.h"
#include "Engine/CurveTable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "CharacterSelector/ZL_Character_Data_Asset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "GameMapsSettings.h"
#include "GameplayEffect.h"
#include "Animation/ZL_DeadlockAnimInstance.h"
#include "Animation/ZL_DeadlockAnimSet.h"
#include "GAS/BaseGameplayAbility.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ZeroLock/ZeroLockCharacter.h"

namespace
{
	const TCHAR* DeadlockContentPath = TEXT("/Game/Developers/rpm86/Deadlock/");
	const TCHAR* DevelopersContentPath = TEXT("/Game/Developers/");
	const TCHAR* FallbackHeroTablePath = TEXT("/Game/ZERO/UI/BP/CharacterSelector/DT_HeroList.DT_HeroList");

	/** Active slots: a missing one is a warning and they should have a cooldown. */
	const FName CooldownSlots[] = { TEXT("SecondryFireAbility"), TEXT("Ability_1"), TEXT("Ability_2"), TEXT("UltimateAbility") };
	const FName RequiredSlots[] = { TEXT("PrimaryFireAbility"), TEXT("SecondryFireAbility"), TEXT("Ability_1"), TEXT("Ability_2"), TEXT("UltimateAbility"), TEXT("ReloadAbility") };

	void AddIssue(TArray<FZL_AuditIssue>& Out, EZL_AuditSeverity Severity, const FString& Hero, const FString& Context, const FString& Message, UObject* Asset)
	{
		FZL_AuditIssue& Issue = Out.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.Hero = Hero;
		Issue.Context = Context;
		Issue.Message = Message;
		Issue.Asset = FSoftObjectPath(Asset);
	}

	/** The Blueprint for a generated class, else the object itself (data assets, montages...). */
	UObject* AssetFor(UObject* Object)
	{
		if (const UClass* Class = Cast<UClass>(Object))
		{
			if (UBlueprint* BP = ZLHeroAudit::GetBlueprint(Class))
			{
				return BP;
			}
		}
		return Object;
	}

	FString NameOf(const UObject* Object)
	{
		if (!Object)
		{
			return TEXT("None");
		}
		if (const UClass* Class = Cast<UClass>(Object))
		{
			if (const UBlueprint* BP = ZLHeroAudit::GetBlueprint(Class))
			{
				return BP->GetName();
			}
		}
		return Object->GetName();
	}

	UDataTable* FindHeroTable()
	{
		const FSoftClassPath GameInstancePath = GetDefault<UGameMapsSettings>()->GameInstanceClass;
		// By name: Zero_BaseGameInstance.h pulls in the online subsystem headers.
		if (const UClass* GIClass = GameInstancePath.TryLoadClass<UObject>())
		{
			if (UDataTable* Table = Cast<UDataTable>(ZLHeroAudit::GetObjectProperty(GIClass->GetDefaultObject(), TEXT("CharacterDataTable"))))
			{
				return Table;
			}
		}
		return LoadObject<UDataTable>(nullptr, FallbackHeroTablePath);
	}

	bool IsUnderPath(const UObject* Object, const TCHAR* Path)
	{
		return Object && Object->GetPathName().StartsWith(Path);
	}

	USkeletalMeshComponent* GetHeroMeshComponent(const AZeroLockCharacter* CDO)
	{
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(ZLHeroAudit::GetObjectProperty(CDO, TEXT("MeshComponent"))))
		{
			return Mesh;
		}
		TArray<UObject*> Subobjects;
		const_cast<AZeroLockCharacter*>(CDO)->GetDefaultSubobjects(Subobjects);
		for (UObject* Sub : Subobjects)
		{
			if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Sub))
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	void CheckSkeleton(TArray<FZL_AuditIssue>& Out, const FString& Hero, const FString& Context, UAnimationAsset* Anim, const USkeleton* MeshSkeleton)
	{
		const USkeleton* AnimSkeleton = Anim ? Anim->GetSkeleton() : nullptr;
		if (!AnimSkeleton || !MeshSkeleton || AnimSkeleton == MeshSkeleton || MeshSkeleton->IsCompatibleForEditor(AnimSkeleton))
		{
			return;
		}
		AddIssue(Out, EZL_AuditSeverity::Error, Hero, Context, FString::Printf(TEXT("%s uses skeleton %s, the hero mesh uses %s (another hero's animation?)"),
			*Anim->GetName(), *AnimSkeleton->GetName(), *MeshSkeleton->GetName()), Anim);
	}

	void AuditMesh(FZL_HeroEntry& Hero, const AZeroLockCharacter* CDO)
	{
		TArray<FZL_AuditIssue>& Out = Hero.Issues;
		const USkeletalMeshComponent* MeshComp = GetHeroMeshComponent(CDO);
		USkeletalMesh* Mesh = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh)
		{
			AddIssue(Out, EZL_AuditSeverity::Error, Hero.Name, TEXT("Mesh"), TEXT("No skeletal mesh on the hero Blueprint"), Hero.Blueprint.Get());
			return;
		}
		const USkeleton* MeshSkeleton = Mesh->GetSkeleton();

		if (!MeshComp->AnimClass)
		{
			AddIssue(Out, EZL_AuditSeverity::Warning, Hero.Name, TEXT("Mesh"), TEXT("Mesh has no Anim Class"), Hero.Blueprint.Get());
		}

		UZL_DeadlockAnimSet* AnimSet = Cast<UZL_DeadlockAnimSet>(ZLHeroAudit::GetObjectProperty(CDO, TEXT("DeadlockAnimSet")));
		if (MeshComp->AnimClass && MeshComp->AnimClass->IsChildOf(UZL_DeadlockAnimInstance::StaticClass()) && !AnimSet)
		{
			const UZL_DeadlockAnimInstance* AnimCDO = GetDefault<UZL_DeadlockAnimInstance>(MeshComp->AnimClass);
			if (!ZLHeroAudit::GetObjectProperty(AnimCDO, TEXT("AnimSet")))
			{
				AddIssue(Out, EZL_AuditSeverity::Error, Hero.Name, TEXT("Mesh"), TEXT("Deadlock anim blueprint but no Deadlock Anim Set"), Hero.Blueprint.Get());
			}
		}

		CheckSkeleton(Out, Hero.Name, TEXT("Light Melee Montage"), CDO->LightMeleeMontage, MeshSkeleton);
		CheckSkeleton(Out, Hero.Name, TEXT("Heavy Melee Montage"), CDO->HeavyMeleeMontage, MeshSkeleton);

		if (AnimSet)
		{
			int32 Missing = 0;
			for (TFieldIterator<FObjectPropertyBase> It(AnimSet->GetClass()); It; ++It)
			{
				if (!It->PropertyClass || !It->PropertyClass->IsChildOf(UAnimationAsset::StaticClass()))
				{
					continue;
				}
				UAnimationAsset* Anim = Cast<UAnimationAsset>(It->GetObjectPropertyValue_InContainer(AnimSet));
				if (!Anim)
				{
					++Missing;
					continue;
				}
				CheckSkeleton(Out, Hero.Name, FString::Printf(TEXT("Anim Set %s"), *It->GetName()), Anim, MeshSkeleton);
			}
			if (Missing > 0)
			{
				AddIssue(Out, EZL_AuditSeverity::Info, Hero.Name, TEXT("Mesh"), FString::Printf(TEXT("%s has %d empty clip slots"), *AnimSet->GetName(), Missing), AnimSet);
			}
		}

		if (const UZL_Character_Data_Asset* Data = Hero.Data.Get())
		{
			if (Data->DisplaySeletalMesh && Data->DisplaySeletalMesh != Mesh)
			{
				AddIssue(Out, EZL_AuditSeverity::Info, Hero.Name, TEXT("Mesh"), FString::Printf(TEXT("Hero select shows %s, the hero uses %s"),
					*Data->DisplaySeletalMesh->GetName(), *Mesh->GetName()), const_cast<UZL_Character_Data_Asset*>(Data));
			}
		}
	}

	void AuditWeapon(FZL_HeroEntry& Hero, const AZeroLockCharacter* CDO)
	{
		TArray<FZL_AuditIssue>& Out = Hero.Issues;
		const UClass* FireClass = CDO->PrimaryFireAbility;
		if (!FireClass)
		{
			return; // reported as a missing slot
		}
		const UObject* FireCDO = FireClass->GetDefaultObject();
		if (!FindFProperty<FProperty>(FireClass, TEXT("ProjectileClass")))
		{
			AddIssue(Out, EZL_AuditSeverity::Info, Hero.Name, TEXT("Weapon"), FString::Printf(TEXT("%s is not a UZL_GA_WeaponFire (legacy weapon?)"), *NameOf(FireClass)), AssetFor(const_cast<UClass*>(FireClass)));
			return;
		}
		UClass* ProjectileClass = ZLHeroAudit::GetClassProperty(FireCDO, TEXT("ProjectileClass"));
		if (!ProjectileClass)
		{
			AddIssue(Out, EZL_AuditSeverity::Error, Hero.Name, TEXT("Weapon"), TEXT("Primary fire has no ProjectileClass"), AssetFor(const_cast<UClass*>(FireClass)));
			return;
		}
		const UObject* ProjCDO = ProjectileClass->GetDefaultObject();
		bool bApplyWeaponDamage = false;
		float WeaponDamage = 0.f;
		ZLHeroAudit::GetBoolProperty(ProjCDO, TEXT("bApplyWeaponDamage"), bApplyWeaponDamage);
		ZLHeroAudit::GetFloatProperty(ProjCDO, TEXT("WeaponDamage"), WeaponDamage);
		const bool bHasImpactEffect = ZLHeroAudit::GetClassProperty(ProjCDO, TEXT("ImpactGameplayEffect")) != nullptr;
		if (!bHasImpactEffect && (!bApplyWeaponDamage || WeaponDamage <= 0.f))
		{
			AddIssue(Out, EZL_AuditSeverity::Warning, Hero.Name, TEXT("Weapon"), FString::Printf(TEXT("%s deals no damage (bApplyWeaponDamage off or WeaponDamage 0, no ImpactGameplayEffect)"),
				*NameOf(ProjectileClass)), AssetFor(ProjectileClass));
		}
	}

	void AuditData(FZL_HeroEntry& Hero)
	{
		TArray<FZL_AuditIssue>& Out = Hero.Issues;
		UZL_Character_Data_Asset* Data = Hero.Data.Get();
		if (!Data)
		{
			AddIssue(Out, EZL_AuditSeverity::Info, Hero.Name, TEXT("Hero list"), TEXT("Not in the hero table: can't be picked in hero select"), Hero.Blueprint.Get());
			return;
		}
		if (!Data->Icon)
		{
			AddIssue(Out, EZL_AuditSeverity::Warning, Hero.Name, TEXT("Data asset"), TEXT("No Icon"), Data);
		}
		if (Data->CharacterDescription.IsEmpty())
		{
			AddIssue(Out, EZL_AuditSeverity::Info, Hero.Name, TEXT("Data asset"), TEXT("No CharacterDescription"), Data);
		}
		if (!Data->DisplaySeletalMesh)
		{
			AddIssue(Out, EZL_AuditSeverity::Warning, Hero.Name, TEXT("Data asset"), TEXT("No DisplaySeletalMesh: hero select shows nothing"), Data);
		}
		if (!Data->DisplayAnimation)
		{
			AddIssue(Out, EZL_AuditSeverity::Info, Hero.Name, TEXT("Data asset"), TEXT("No DisplayAnimation"), Data);
		}
		if (!Data->CharacterClass)
		{
			AddIssue(Out, EZL_AuditSeverity::Error, Hero.Name, TEXT("Data asset"), TEXT("No CharacterClass: picking this hero spawns nothing"), Data);
		}
	}

	FString FormatPropertyValue(const FProperty* Property, const void* Container)
	{
		if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
		{
			if (Struct->Struct == FScalableFloat::StaticStruct())
			{
				const FScalableFloat* Value = Struct->ContainerPtrToValuePtr<FScalableFloat>(Container);
				return Value->Curve.CurveTable ? FString::Printf(TEXT("%g*%s"), Value->Value, *Value->Curve.RowName.ToString()) : FString::Printf(TEXT("%g"), Value->Value);
			}
		}
		FString Text;
		Property->ExportText_InContainer(0, Text, Container, nullptr, nullptr, PPF_None);
		return Text;
	}

	FString CsvEscape(const FString& In)
	{
		if (In.Contains(TEXT(",")) || In.Contains(TEXT("\"")) || In.Contains(TEXT("\n")))
		{
			return FString::Printf(TEXT("\"%s\""), *In.Replace(TEXT("\""), TEXT("\"\"")));
		}
		return In;
	}

	FString CsvRow(std::initializer_list<FString> Cells)
	{
		TArray<FString> Escaped;
		for (const FString& Cell : Cells)
		{
			Escaped.Add(CsvEscape(Cell));
		}
		return FString::Join(Escaped, TEXT(",")) + LINE_TERMINATOR;
	}
}

int32 FZL_HeroEntry::CountIssues(EZL_AuditSeverity Severity) const
{
	int32 Count = 0;
	for (const FZL_AuditIssue& Issue : Issues)
	{
		Count += Issue.Severity == Severity ? 1 : 0;
	}
	return Count;
}

AZeroLockCharacter* FZL_HeroEntry::GetHeroCDO() const
{
	return HeroClass ? HeroClass->GetDefaultObject<AZeroLockCharacter>() : nullptr;
}

namespace ZLHeroAudit
{
	UBlueprint* GetBlueprint(const UClass* Class)
	{
		return Class ? Cast<UBlueprint>(Class->ClassGeneratedBy) : nullptr;
	}

	FString SeverityToString(EZL_AuditSeverity Severity)
	{
		switch (Severity)
		{
		case EZL_AuditSeverity::Error: return TEXT("Error");
		case EZL_AuditSeverity::Warning: return TEXT("Warning");
		default: return TEXT("Info");
		}
	}

	FLinearColor SeverityColor(EZL_AuditSeverity Severity)
	{
		switch (Severity)
		{
		case EZL_AuditSeverity::Error: return FLinearColor(0.95f, 0.25f, 0.2f);
		case EZL_AuditSeverity::Warning: return FLinearColor(1.f, 0.7f, 0.15f);
		default: return FLinearColor(0.55f, 0.7f, 0.9f);
		}
	}

	UObject* GetObjectProperty(const UObject* Object, FName Name)
	{
		const FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
	}

	UClass* GetClassProperty(const UObject* Object, FName Name)
	{
		return Cast<UClass>(GetObjectProperty(Object, Name));
	}

	bool GetFloatProperty(const UObject* Object, FName Name, float& OutValue)
	{
		const FProperty* Property = Object ? FindFProperty<FProperty>(Object->GetClass(), Name) : nullptr;
		if (const FFloatProperty* Float = CastField<FFloatProperty>(Property))
		{
			OutValue = Float->GetPropertyValue_InContainer(Object);
			return true;
		}
		if (const FDoubleProperty* Double = CastField<FDoubleProperty>(Property))
		{
			OutValue = static_cast<float>(Double->GetPropertyValue_InContainer(Object));
			return true;
		}
		return false;
	}

	bool GetBoolProperty(const UObject* Object, FName Name, bool& OutValue)
	{
		const FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property)
		{
			return false;
		}
		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	float GetCooldown(const UBaseGameplayAbility* Ability)
	{
		return Ability ? Ability->CooldownDuration.GetValueAtLevel(1) : 0.f;
	}

	FString DescribeTunables(const UObject* Object, const UClass* StopAtClass)
	{
		if (!Object)
		{
			return FString();
		}
		TArray<FString> Parts;
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			const UClass* Owner = Property->GetOwnerClass();
			if (!Owner || (StopAtClass && !Owner->IsChildOf(StopAtClass)) || Owner == StopAtClass
				|| !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}
			const FStructProperty* Struct = CastField<FStructProperty>(Property);
			const bool bTunable = Property->IsA<FNumericProperty>() || Property->IsA<FBoolProperty>() || Property->IsA<FEnumProperty>()
				|| Property->IsA<FNameProperty>() || (Struct && Struct->Struct == FScalableFloat::StaticStruct());
			if (bTunable)
			{
				Parts.Add(FString::Printf(TEXT("%s=%s"), *Property->GetName(), *FormatPropertyValue(Property, Object)));
			}
		}
		return FString::Join(Parts, TEXT("; "));
	}

	void AuditAbility(UClass* AbilityClass, const FString& Hero, const FString& SlotLabel, bool bExpectsCooldown, TArray<FZL_AuditIssue>& Out)
	{
		const UBaseGameplayAbility* CDO = AbilityClass ? AbilityClass->GetDefaultObject<UBaseGameplayAbility>() : nullptr;
		if (!CDO)
		{
			return;
		}
		UObject* Asset = AssetFor(AbilityClass);
		const FString Context = SlotLabel.IsEmpty() ? NameOf(AbilityClass) : FString::Printf(TEXT("%s (%s)"), *SlotLabel, *NameOf(AbilityClass));

		if (bExpectsCooldown)
		{
			if (!CDO->IconImage)
			{
				AddIssue(Out, EZL_AuditSeverity::Warning, Hero, Context, TEXT("No IconImage: the HUD slot is blank"), Asset);
			}
			if (CDO->AbilityName.IsEmpty())
			{
				AddIssue(Out, EZL_AuditSeverity::Warning, Hero, Context, TEXT("No AbilityName"), Asset);
			}
			if (CDO->AbilityDescription.IsEmpty())
			{
				AddIssue(Out, EZL_AuditSeverity::Info, Hero, Context, TEXT("No AbilityDescription"), Asset);
			}
			if (GetCooldown(CDO) <= 0.f && !CDO->bIsChargedAbility)
			{
				AddIssue(Out, EZL_AuditSeverity::Warning, Hero, Context, TEXT("CooldownDuration is 0"), Asset);
			}
		}
		if (GetCooldown(CDO) > 0.f && CDO->CooldownTags.IsEmpty())
		{
			AddIssue(Out, EZL_AuditSeverity::Error, Hero, Context, TEXT("CooldownDuration set but no CooldownTags: the cooldown never blocks the ability or shows on the HUD"), Asset);
		}
		if (CDO->bIsChargedAbility && CDO->TimeBetweenCharges <= 0.f)
		{
			AddIssue(Out, EZL_AuditSeverity::Warning, Hero, Context, TEXT("Charged ability with TimeBetweenCharges 0"), Asset);
		}
		if (SlotLabel == TEXT("Ultimate Ability") && CDO->Slot != EGameplayAbilitySlot::UltimateSlot)
		{
			AddIssue(Out, EZL_AuditSeverity::Warning, Hero, Context, TEXT("In the ultimate slot but Slot isn't Ultimate Slot"), Asset);
		}

		// Scalable floats pointing at a curve table row that doesn't exist evaluate to 0.
		for (TFieldIterator<FStructProperty> It(AbilityClass); It; ++It)
		{
			if (It->Struct != FScalableFloat::StaticStruct())
			{
				continue;
			}
			const FScalableFloat* Value = It->ContainerPtrToValuePtr<FScalableFloat>(CDO);
			if (Value->Curve.CurveTable && !Value->Curve.CurveTable->FindCurveUnchecked(Value->Curve.RowName))
			{
				AddIssue(Out, EZL_AuditSeverity::Error, Hero, Context, FString::Printf(TEXT("%s uses row '%s', which %s doesn't have: the value is 0"),
					*It->GetName(), *Value->Curve.RowName.ToString(), *Value->Curve.CurveTable->GetName()), Asset);
			}
		}

		// Debug drawing left on: any editable bool with "Debug" in its name.
		for (TFieldIterator<FBoolProperty> It(AbilityClass); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Edit) && It->GetName().Contains(TEXT("Debug")) && It->GetPropertyValue_InContainer(CDO))
			{
				AddIssue(Out, EZL_AuditSeverity::Warning, Hero, Context, FString::Printf(TEXT("%s is on"), *It->GetName()), Asset);
			}
		}
		// The same for the projectile the ability spawns, if it has one.
		for (const FName ProjectileProp : { FName(TEXT("ProjectileClass")), FName(TEXT("ServerProjectileClass")) })
		{
			const UClass* Projectile = GetClassProperty(CDO, ProjectileProp);
			const UObject* ProjCDO = Projectile ? Projectile->GetDefaultObject() : nullptr;
			bool bDebug = false;
			if (ProjCDO && GetBoolProperty(ProjCDO, TEXT("bDrawDebug"), bDebug) && bDebug)
			{
				AddIssue(Out, EZL_AuditSeverity::Warning, Hero, Context, FString::Printf(TEXT("%s has bDrawDebug on"), *NameOf(Projectile)), AssetFor(const_cast<UClass*>(Projectile)));
			}
		}
	}

	void AuditHero(FZL_HeroEntry& Hero)
	{
		Hero.Issues.Reset();
		Hero.Slots.Reset();
		AuditData(Hero);

		const AZeroLockCharacter* CDO = Hero.GetHeroCDO();
		if (!CDO)
		{
			return;
		}

		// Every ability property on the hero class, native or Blueprint, in declaration order.
		for (TFieldIterator<FProperty> It(Hero.HeroClass); It; ++It)
		{
			const FProperty* Property = *It;
			if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
			{
				if (!ClassProp->MetaClass || !ClassProp->MetaClass->IsChildOf(UBaseGameplayAbility::StaticClass()))
				{
					continue;
				}
				FZL_AbilitySlot& Slot = Hero.Slots.AddDefaulted_GetRef();
				Slot.PropertyName = Property->GetFName();
				Slot.Label = FName::NameToDisplayString(Property->GetName(), false);
				Slot.AbilityClass = Cast<UClass>(ClassProp->GetObjectPropertyValue_InContainer(CDO));
				for (const FName& Name : CooldownSlots)
				{
					Slot.bExpectsCooldown |= Name == Slot.PropertyName;
				}
			}
			else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				const FClassProperty* Inner = CastField<FClassProperty>(ArrayProp->Inner);
				if (!Inner || !Inner->MetaClass || !Inner->MetaClass->IsChildOf(UGameplayAbility::StaticClass()))
				{
					continue;
				}
				FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CDO));
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					FZL_AbilitySlot& Slot = Hero.Slots.AddDefaulted_GetRef();
					Slot.PropertyName = Property->GetFName();
					Slot.Label = FString::Printf(TEXT("%s [%d]"), *FName::NameToDisplayString(Property->GetName(), false), Index);
					Slot.bFromArray = true;
					Slot.AbilityClass = Cast<UClass>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index)));
				}
			}
		}

		Hero.Slots.StableSort([](const FZL_AbilitySlot& A, const FZL_AbilitySlot& B) { return !A.bFromArray && B.bFromArray; });

		TMap<UClass*, FString> SeenAbilities;
		for (FZL_AbilitySlot& Slot : Hero.Slots)
		{
			bool bRequired = false;
			for (const FName& Name : RequiredSlots)
			{
				bRequired |= Name == Slot.PropertyName;
			}
			if (!Slot.AbilityClass)
			{
				if (bRequired)
				{
					AddIssue(Slot.Issues, EZL_AuditSeverity::Warning, Hero.Name, Slot.Label, TEXT("Empty"), Hero.Blueprint.Get());
				}
				continue;
			}
			if (const FString* Other = SeenAbilities.Find(Slot.AbilityClass.Get()))
			{
				AddIssue(Slot.Issues, EZL_AuditSeverity::Warning, Hero.Name, Slot.Label, FString::Printf(TEXT("Same ability as %s"), **Other), AssetFor(Slot.AbilityClass));
			}
			SeenAbilities.Add(Slot.AbilityClass.Get(), Slot.Label);
			AuditAbility(Slot.AbilityClass, Hero.Name, Slot.Label, Slot.bExpectsCooldown, Slot.Issues);
			Hero.Issues.Append(Slot.Issues);
		}
		// Empty required slots aren't appended above.
		for (const FZL_AbilitySlot& Slot : Hero.Slots)
		{
			if (!Slot.AbilityClass)
			{
				Hero.Issues.Append(Slot.Issues);
			}
		}

		AuditMesh(Hero, CDO);
		AuditWeapon(Hero, CDO);

		if (!CDO->DefaultGameplayEffect)
		{
			AddIssue(Hero.Issues, EZL_AuditSeverity::Error, Hero.Name, TEXT("Stats"), TEXT("No DefaultGameplayEffect: the hero starts with no health/ammo attributes"), Hero.Blueprint.Get());
		}
		else
		{
			// Current<X> starting above Max<X> (e.g. CurrentAmmo 50, MaxAmmo 30).
			TMap<FString, float> Values;
			for (const FGameplayModifierInfo& Modifier : CDO->DefaultGameplayEffect->GetDefaultObject<UGameplayEffect>()->Modifiers)
			{
				float Value = 0.f;
				if (Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Value))
				{
					Values.Add(Modifier.Attribute.GetName(), Value);
				}
			}
			for (const TPair<FString, float>& Pair : Values)
			{
				if (!Pair.Key.StartsWith(TEXT("Current")))
				{
					continue;
				}
				const FString MaxName = TEXT("Max") + Pair.Key.RightChop(7);
				const float* Max = Values.Find(MaxName);
				if (Max && Pair.Value > *Max)
				{
					AddIssue(Hero.Issues, EZL_AuditSeverity::Warning, Hero.Name, TEXT("Stats"), FString::Printf(TEXT("Starts with %s %g, above %s %g"),
						*Pair.Key, Pair.Value, *MaxName, *Max), AssetFor(CDO->DefaultGameplayEffect.Get()));
				}
			}
		}
	}

	TArray<TSharedPtr<FZL_HeroEntry>> GatherHeroes(bool bIncludeUnlisted)
	{
		TArray<TSharedPtr<FZL_HeroEntry>> Heroes;
		TSet<const UClass*> Listed;

		if (UDataTable* Table = FindHeroTable())
		{
			Table->ForeachRow<FHeroTableRow>(TEXT("ZLHeroAudit"), [&](const FName& Key, const FHeroTableRow& Row)
			{
				UZL_Character_Data_Asset* Data = Row.HeroAsset.LoadSynchronous();
				TSharedPtr<FZL_HeroEntry> Hero = MakeShared<FZL_HeroEntry>();
				Hero->bInHeroList = true;
				Hero->Data = Data;
				Hero->Name = Data && !Data->CharacterName.IsEmpty() ? Data->CharacterName : Key.ToString();
				if (Data)
				{
					Hero->HeroClass = Data->CharacterClass;
					Hero->Color = FLinearColor(Data->IconColor);
					Hero->Icon = Data->Icon;
				}
				Hero->Blueprint = GetBlueprint(Hero->HeroClass);
				Listed.Add(Hero->HeroClass);
				Heroes.Add(Hero);
			});
		}

		if (bIncludeUnlisted)
		{
			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			TSet<FTopLevelAssetPath> Derived;
			Registry.GetDerivedClassNames({ AZeroLockCharacter::StaticClass()->GetClassPathName() }, {}, Derived);
			TArray<FTopLevelAssetPath> Sorted = Derived.Array();
			Sorted.Sort([](const FTopLevelAssetPath& A, const FTopLevelAssetPath& B) { return A.GetAssetName().LexicalLess(B.GetAssetName()); });
			for (const FTopLevelAssetPath& Path : Sorted)
			{
				const FString PathString = Path.ToString();
				if (!PathString.StartsWith(TEXT("/Game/")) || Path.GetAssetName().ToString().StartsWith(TEXT("SKEL_")) || Path.GetAssetName().ToString().StartsWith(TEXT("REINST_")))
				{
					continue;
				}
				UClass* Class = LoadObject<UClass>(nullptr, *PathString);
				if (!Class || Listed.Contains(Class) || Class->HasAnyClassFlags(CLASS_Abstract))
				{
					continue;
				}
				TSharedPtr<FZL_HeroEntry> Hero = MakeShared<FZL_HeroEntry>();
				Hero->HeroClass = Class;
				Hero->Blueprint = GetBlueprint(Class);
				Hero->Name = Hero->Blueprint.IsValid() ? Hero->Blueprint->GetName() : Class->GetName();
				Heroes.Add(Hero);
			}
		}

		for (const TSharedPtr<FZL_HeroEntry>& Hero : Heroes)
		{
			AuditHero(*Hero);
		}
		return Heroes;
	}

	TArray<UClass*> GetAllAbilityBlueprintClasses()
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TSet<FTopLevelAssetPath> Derived;
		Registry.GetDerivedClassNames({ UBaseGameplayAbility::StaticClass()->GetClassPathName() }, {}, Derived);

		TArray<UClass*> Classes;
		for (const FTopLevelAssetPath& Path : Derived)
		{
			const FString PathString = Path.ToString();
			const FString AssetName = Path.GetAssetName().ToString();
			if (!PathString.StartsWith(TEXT("/Game/")) || AssetName.StartsWith(TEXT("SKEL_")) || AssetName.StartsWith(TEXT("REINST_")))
			{
				continue;
			}
			if (UClass* Class = LoadObject<UClass>(nullptr, *PathString))
			{
				Classes.Add(Class);
			}
		}
		Classes.Sort([](const UClass& A, const UClass& B) { return A.GetPathName() < B.GetPathName(); });
		return Classes;
	}

	TArray<FZL_AuditIssue> AuditProject(const TArray<TSharedPtr<FZL_HeroEntry>>& Heroes)
	{
		TArray<FZL_AuditIssue> Issues;
		TSet<UClass*> UsedAbilities;
		for (const TSharedPtr<FZL_HeroEntry>& Hero : Heroes)
		{
			Issues.Append(Hero->Issues);
			for (const FZL_AbilitySlot& Slot : Hero->Slots)
			{
				UsedAbilities.Add(Slot.AbilityClass.Get());
			}
		}

		// Ability Blueprints no hero uses (still audited, so stale debug flags etc. show up).
		for (UClass* Ability : GetAllAbilityBlueprintClasses())
		{
			if (UsedAbilities.Contains(Ability) || Ability->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}
			// Only Blueprints that look like hero abilities: the rest are bases, items and tests.
			if (Ability->GetPathName().StartsWith(TEXT("/Game/ZERO/HeroAbilities/")))
			{
				AddIssue(Issues, EZL_AuditSeverity::Info, FString(), NameOf(Ability), TEXT("Hero ability Blueprint no hero uses"), AssetFor(Ability));
			}
		}

		// Shipping content (everything outside Developers) must not reference Developers, Deadlock assets above all.
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(TEXT("/Game"), Assets, true);
		TSet<FName> Checked;
		for (const FAssetData& Asset : Assets)
		{
			const FString Package = Asset.PackageName.ToString();
			if (Package.StartsWith(DevelopersContentPath) || Package.Contains(TEXT("/__ExternalActors__/Developers/")) || Package.Contains(TEXT("/__ExternalObjects__/Developers/")))
			{
				continue;
			}
			bool bAlreadyChecked = false;
			Checked.Add(Asset.PackageName, &bAlreadyChecked);
			if (bAlreadyChecked)
			{
				continue;
			}
			TArray<FName> Dependencies;
			Registry.GetDependencies(Asset.PackageName, Dependencies);
			for (const FName& Dependency : Dependencies)
			{
				const FString DepString = Dependency.ToString();
				if (DepString.StartsWith(DeadlockContentPath))
				{
					AddIssue(Issues, EZL_AuditSeverity::Error, FString(), Asset.AssetName.ToString(),
						FString::Printf(TEXT("Shipping content references Deadlock asset %s"), *DepString), nullptr);
					Issues.Last().Asset = Asset.GetSoftObjectPath();
				}
				else if (DepString.StartsWith(DevelopersContentPath))
				{
					AddIssue(Issues, EZL_AuditSeverity::Warning, FString(), Asset.AssetName.ToString(),
						FString::Printf(TEXT("Shipping content references Developers asset %s"), *DepString), nullptr);
				}
			}
		}
		return Issues;
	}

	bool ExportCSV(const FString& Dir, const TArray<TSharedPtr<FZL_HeroEntry>>& Heroes, const TArray<FZL_AuditIssue>& Issues, FString& OutError)
	{
		TMap<UClass*, TArray<FString>> UsedBy;
		FString Slots = CsvRow({ TEXT("Hero"), TEXT("InHeroList"), TEXT("HeroBlueprint"), TEXT("Slot"), TEXT("Ability"), TEXT("Cooldown"), TEXT("Issues") });
		for (const TSharedPtr<FZL_HeroEntry>& Hero : Heroes)
		{
			for (const FZL_AbilitySlot& Slot : Hero->Slots)
			{
				const UBaseGameplayAbility* CDO = Slot.AbilityClass ? Slot.AbilityClass->GetDefaultObject<UBaseGameplayAbility>() : nullptr;
				TArray<FString> Messages;
				for (const FZL_AuditIssue& Issue : Slot.Issues)
				{
					Messages.Add(Issue.Message);
				}
				Slots += CsvRow({ Hero->Name, Hero->bInHeroList ? TEXT("yes") : TEXT("no"), NameOf(Hero->HeroClass), Slot.Label, NameOf(Slot.AbilityClass),
					CDO ? FString::Printf(TEXT("%g"), GetCooldown(CDO)) : FString(), FString::Join(Messages, TEXT(" | ")) });
				if (Slot.AbilityClass)
				{
					UsedBy.FindOrAdd(Slot.AbilityClass.Get()).Add(FString::Printf(TEXT("%s:%s"), *Hero->Name, *Slot.Label));
				}
			}
		}

		FString Abilities = CsvRow({ TEXT("Ability"), TEXT("Path"), TEXT("Parent"), TEXT("UsedBy"), TEXT("Name"), TEXT("Slot"), TEXT("Cooldown"), TEXT("CooldownTags"),
			TEXT("Charged"), TEXT("MaxCharges"), TEXT("TimeBetweenCharges"), TEXT("MovementAbility"), TEXT("NetExecution"), TEXT("Icon"), TEXT("Issues"), TEXT("Tunables") });
		const UEnum* SlotEnum = StaticEnum<EGameplayAbilitySlot>();
		const UEnum* NetEnum = StaticEnum<EGameplayAbilityNetExecutionPolicy::Type>();
		for (UClass* Class : GetAllAbilityBlueprintClasses())
		{
			const UBaseGameplayAbility* CDO = Class->GetDefaultObject<UBaseGameplayAbility>();
			const TArray<FString>* Users = UsedBy.Find(Class);
			TArray<FZL_AuditIssue> AbilityIssues;
			AuditAbility(Class, FString(), FString(), Users != nullptr && Class->GetPathName().StartsWith(TEXT("/Game/ZERO/HeroAbilities/")), AbilityIssues);
			TArray<FString> Messages;
			for (const FZL_AuditIssue& Issue : AbilityIssues)
			{
				Messages.Add(Issue.Message);
			}
			const UClass* Native = Class;
			while (Native && !Native->HasAnyClassFlags(CLASS_Native))
			{
				Native = Native->GetSuperClass();
			}
			Abilities += CsvRow({ NameOf(Class), Class->GetPathName(), Native ? Native->GetName() : FString(), Users ? FString::Join(*Users, TEXT(" | ")) : FString(),
				CDO->AbilityName, SlotEnum->GetNameStringByValue(static_cast<int64>(CDO->Slot)), FString::Printf(TEXT("%g"), GetCooldown(CDO)), CDO->CooldownTags.ToStringSimple(),
				CDO->bIsChargedAbility ? TEXT("yes") : TEXT("no"), FString::Printf(TEXT("%g"), CDO->MaxChargesConfig), FString::Printf(TEXT("%g"), CDO->TimeBetweenCharges),
				CDO->bIsMovementAbility ? TEXT("yes") : TEXT("no"), NetEnum ? NetEnum->GetNameStringByValue(CDO->GetNetExecutionPolicy()) : FString(),
				CDO->IconImage ? CDO->IconImage->GetName() : FString(), FString::Join(Messages, TEXT(" | ")), DescribeTunables(CDO, UBaseGameplayAbility::StaticClass()) });
		}

		FString IssueCsv = CsvRow({ TEXT("Severity"), TEXT("Hero"), TEXT("Context"), TEXT("Message"), TEXT("Asset") });
		for (const FZL_AuditIssue& Issue : Issues)
		{
			IssueCsv += CsvRow({ SeverityToString(Issue.Severity), Issue.Hero, Issue.Context, Issue.Message, Issue.Asset.ToString() });
		}

		const TPair<const TCHAR*, FString*> Files[] = { { TEXT("AbilityAudit.csv"), &Abilities }, { TEXT("HeroSlots.csv"), &Slots }, { TEXT("Issues.csv"), &IssueCsv } };
		for (const TPair<const TCHAR*, FString*>& File : Files)
		{
			const FString Path = FPaths::Combine(Dir, File.Key);
			if (!FFileHelper::SaveStringToFile(*File.Value, *Path, FFileHelper::EEncodingOptions::ForceUTF8))
			{
				OutError = FString::Printf(TEXT("Couldn't write %s (open in Excel?)"), *Path);
				return false;
			}
		}
		return true;
	}
}

namespace ZLHeroAudit
{
	namespace
	{
		const TCHAR* HeroAbilitiesPath = TEXT("/Game/ZERO/HeroAbilities/");

		bool IsHeroOwnAbility(const UClass* Class)
		{
			return Class && Class->GetPathName().StartsWith(HeroAbilitiesPath);
		}

		FString AbilityShortName(const UClass* Class, const FString& HeroShort)
		{
			const UBlueprint* BP = GetBlueprint(Class);
			FString Name = BP ? BP->GetName() : Class->GetName();
			Name.RemoveFromStart(TEXT("BP_"));
			Name.RemoveFromStart(HeroShort + TEXT("_"));
			Name.RemoveFromEnd(TEXT("_C"));
			return Name;
		}

		FName RowNameFor(const FZL_CurveLink& Link)
		{
			// CooldownDuration -> "<Ability>_Cooldown", like the existing tables.
			const FString Property = Link.Property == TEXT("CooldownDuration") ? FString(TEXT("Cooldown")) : Link.Property.ToString();
			return FName(*FString::Printf(TEXT("%s_%s"), *Link.AbilityShortName, *Property));
		}

		void BroadcastTableChange(UCurveTable* Table, bool bPost)
		{
			if (bPost)
			{
				FCurveTableEditorUtils::BroadcastPostChange(Table, FCurveTableEditorUtils::ECurveTableChangeInfo::RowList);
				Table->MarkPackageDirty();
			}
			else
			{
				Table->Modify();
				FCurveTableEditorUtils::BroadcastPreChange(Table, FCurveTableEditorUtils::ECurveTableChangeInfo::RowList);
			}
		}
	}

	void MarkClassDefaultsEdited(UObject* Object)
	{
		if (!Object)
		{
			return;
		}
		Object->MarkPackageDirty();
		for (UObject* Outer = Object; Outer; Outer = Outer->GetOuter())
		{
			if (Outer->HasAnyFlags(RF_ClassDefaultObject))
			{
				if (UBlueprint* BP = GetBlueprint(Outer->GetClass()))
				{
					FBlueprintEditorUtils::PostEditChangeBlueprintActors(BP);
				}
				break;
			}
		}
	}

	FString GetHeroShortName(const FZL_HeroEntry& Hero)
	{
		FString Name = Hero.Name;
		Name.RemoveFromStart(TEXT("The "));
		FString Out;
		for (const TCHAR Char : Name)
		{
			if (FChar::IsAlnum(Char))
			{
				Out.AppendChar(Char);
			}
		}
		return Out.IsEmpty() ? FString(TEXT("Hero")) : Out;
	}

	FString GetHeroContentFolder(const FZL_HeroEntry& Hero)
	{
		TMap<FString, int32> Counts;
		for (const FZL_AbilitySlot& Slot : Hero.Slots)
		{
			if (!IsHeroOwnAbility(Slot.AbilityClass))
			{
				continue;
			}
			const FString Rest = Slot.AbilityClass->GetPathName().RightChop(FCString::Strlen(HeroAbilitiesPath));
			FString Folder;
			if (Rest.Split(TEXT("/"), &Folder, nullptr))
			{
				++Counts.FindOrAdd(Folder);
			}
		}
		FString Best;
		int32 BestCount = 0;
		for (const TPair<FString, int32>& Pair : Counts)
		{
			if (Pair.Value > BestCount)
			{
				Best = Pair.Key;
				BestCount = Pair.Value;
			}
		}
		return FString(HeroAbilitiesPath) + (Best.IsEmpty() ? GetHeroShortName(Hero) : Best);
	}

	TArray<FZL_CurveLink> GetHeroScalableFloats(const FZL_HeroEntry& Hero)
	{
		TArray<FZL_CurveLink> Links;
		TSet<UClass*> Seen;
		const FString HeroShort = GetHeroShortName(Hero);
		for (const FZL_AbilitySlot& Slot : Hero.Slots)
		{
			UClass* Class = Slot.AbilityClass.Get();
			if (!IsHeroOwnAbility(Class) || Seen.Contains(Class))
			{
				continue;
			}
			Seen.Add(Class);
			const UObject* CDO = Class->GetDefaultObject();
			for (TFieldIterator<FStructProperty> It(Class); It; ++It)
			{
				if (It->Struct != FScalableFloat::StaticStruct() || !It->HasAnyPropertyFlags(CPF_Edit))
				{
					continue;
				}
				const FScalableFloat* Value = It->ContainerPtrToValuePtr<FScalableFloat>(CDO);
				FZL_CurveLink& Link = Links.AddDefaulted_GetRef();
				Link.AbilityClass = Class;
				Link.Property = It->GetFName();
				Link.AbilityShortName = AbilityShortName(Class, HeroShort);
				Link.Table = const_cast<UCurveTable*>(ToRawPtr(Value->Curve.CurveTable));
				Link.Row = Value->Curve.RowName;
				Link.Value = Value->Value;
				Link.bRowMissing = Value->Curve.CurveTable && !Value->Curve.CurveTable->FindCurveUnchecked(Value->Curve.RowName);
			}
		}
		return Links;
	}

	UCurveTable* FindHeroCurveTable(const FZL_HeroEntry& Hero)
	{
		TMap<UCurveTable*, int32> Counts;
		for (const FZL_CurveLink& Link : GetHeroScalableFloats(Hero))
		{
			if (UCurveTable* Table = Link.Table.Get())
			{
				++Counts.FindOrAdd(Table);
			}
		}
		UCurveTable* Best = nullptr;
		int32 BestCount = 0;
		for (const TPair<UCurveTable*, int32>& Pair : Counts)
		{
			if (Pair.Value > BestCount)
			{
				Best = Pair.Key;
				BestCount = Pair.Value;
			}
		}
		if (!Best)
		{
			// Not linked yet, but C_<Hero> may already exist.
			const FString Name = TEXT("C_") + GetHeroShortName(Hero);
			Best = LoadObject<UCurveTable>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *GetHeroContentFolder(Hero), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return Best;
	}

	UCurveTable* CreateHeroCurveTable(const FZL_HeroEntry& Hero, FString& OutError)
	{
		const FString Name = TEXT("C_") + GetHeroShortName(Hero);
		const FString PackageName = GetHeroContentFolder(Hero) + TEXT("/") + Name;
		if (UCurveTable* Existing = LoadObject<UCurveTable>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			OutError = FString::Printf(TEXT("Couldn't create package %s"), *PackageName);
			return nullptr;
		}
		UCurveTable* Table = NewObject<UCurveTable>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Table);
		Table->MarkPackageDirty();
		return Table;
	}

	bool AddCurveTableRow(UCurveTable* Table, FName Row, float Value, int32 NumLevels)
	{
		if (!Table || Row.IsNone() || Table->FindCurveUnchecked(Row))
		{
			return false;
		}
		BroadcastTableChange(Table, false);
		FRealCurve* Curve = Table->GetCurveTableMode() == ECurveTableMode::RichCurves
			? static_cast<FRealCurve*>(&Table->AddRichCurve(Row))
			: static_cast<FRealCurve*>(&Table->AddSimpleCurve(Row));
		for (int32 Level = 0; Level < NumLevels; ++Level)
		{
			Curve->UpdateOrAddKey(static_cast<float>(Level), Value);
		}
		BroadcastTableChange(Table, true);
		return true;
	}

	void RemoveCurveTableRow(UCurveTable* Table, FName Row)
	{
		if (!Table || !Table->FindCurveUnchecked(Row))
		{
			return;
		}
		BroadcastTableChange(Table, false);
		Table->RemoveRow(Row);
		BroadcastTableChange(Table, true);
	}

	void SetCurveTableValue(UCurveTable* Table, FName Row, float Level, float Value)
	{
		FRealCurve* Curve = Table ? Table->FindCurveUnchecked(Row) : nullptr;
		if (!Curve)
		{
			return;
		}
		BroadcastTableChange(Table, false);
		const FKeyHandle Key = Curve->FindKey(Level);
		if (Curve->IsKeyHandleValid(Key))
		{
			Curve->SetKeyValue(Key, Value);
		}
		else
		{
			Curve->UpdateOrAddKey(Level, Value);
		}
		BroadcastTableChange(Table, true);
	}

	float GetCurveTableValue(const UCurveTable* Table, FName Row, float Level)
	{
		const FRealCurve* Curve = Table ? Table->FindCurveUnchecked(Row) : nullptr;
		return Curve ? Curve->Eval(Level) : 0.f;
	}

	int32 LinkHeroValuesToTable(const FZL_HeroEntry& Hero, UCurveTable* Table, int32 NumLevels)
	{
		if (!Table)
		{
			return 0;
		}
		int32 Linked = 0;
		for (const FZL_CurveLink& Link : GetHeroScalableFloats(Hero))
		{
			if (Link.Table.IsValid() || FMath::IsNearlyZero(Link.Value))
			{
				continue;
			}
			const FName Row = RowNameFor(Link);
			AddCurveTableRow(Table, Row, Link.Value, NumLevels); // no-op when the row exists

			UObject* CDO = Link.AbilityClass->GetDefaultObject();
			const FStructProperty* Property = FindFProperty<FStructProperty>(Link.AbilityClass, Link.Property);
			FScalableFloat* Value = Property->ContainerPtrToValuePtr<FScalableFloat>(CDO);
			CDO->Modify();
			Value->Value = 1.f;
			Value->Curve.CurveTable = Table;
			Value->Curve.RowName = Row;
			MarkClassDefaultsEdited(CDO);
			++Linked;
		}
		return Linked;
	}

	UClass* CopyDefaultEffectForHero(const FZL_HeroEntry& Hero, FString& OutError)
	{
		AZeroLockCharacter* HeroCDO = Hero.GetHeroCDO();
		UBlueprint* SourceBP = HeroCDO ? GetBlueprint(HeroCDO->DefaultGameplayEffect) : nullptr;
		if (!SourceBP)
		{
			OutError = TEXT("The hero has no DefaultGameplayEffect Blueprint to copy");
			return nullptr;
		}
		const FString Folder = GetHeroContentFolder(Hero);
		const FString Name = FString::Printf(TEXT("GE_%s_DefaultStats"), *GetHeroShortName(Hero));

		UBlueprint* NewBP = LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!NewBP)
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
			NewBP = Cast<UBlueprint>(AssetTools.DuplicateAsset(Name, Folder, SourceBP));
		}
		if (!NewBP)
		{
			OutError = FString::Printf(TEXT("Couldn't duplicate %s to %s/%s"), *SourceBP->GetName(), *Folder, *Name);
			return nullptr;
		}
		if (!NewBP->GeneratedClass)
		{
			FKismetEditorUtilities::CompileBlueprint(NewBP);
		}
		HeroCDO->Modify();
		HeroCDO->DefaultGameplayEffect = NewBP->GeneratedClass.Get();
		MarkClassDefaultsEdited(HeroCDO);
		return NewBP->GeneratedClass;
	}
}
