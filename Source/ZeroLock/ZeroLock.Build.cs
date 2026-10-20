// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZeroLock : ModuleRules
{
	public ZeroLock(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "CommonUI", "Mover" });
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"Paper2D",
			"NetCore",
			"AIModule",
			"Niagara",
			"NavigationSystem",
			"UMG",
			"CommonUI",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"CommonUser",
			"HTTP",
			"OpenSSL",
			"Json",
			"ModelViewViewModel",
			"SlateMVVM", "Niagara", "Niagara"
		});
	}
}
