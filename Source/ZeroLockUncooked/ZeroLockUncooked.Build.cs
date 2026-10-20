// Copyright Preetham Mukundan (C) 2026

using UnrealBuildTool;

// UncookedOnly: loads in the editor AND in uncooked -game runs (UnrealEditor.exe -game), unlike Editor modules.
// Anim graph nodes must live here: an uncooked game recompiles anim blueprints on load, and a missing node class
// leaves the AnimGraph empty (every Deadlock hero T-posed in -game while ABP_DeadlockHero's node was in ZeroEditorModule).
// Keep editor-UI code (LevelEditor, Blutility, ...) out of this module.
public class ZeroLockUncooked : ModuleRules
{
	public ZeroLockUncooked(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ZeroLock",
				"AnimGraph",
				"AnimGraphRuntime",
				"BlueprintGraph",
			}
		);
	}
}
