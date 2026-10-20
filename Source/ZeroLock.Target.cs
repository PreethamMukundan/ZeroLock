// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ZeroLockTarget : TargetRules
{
	public ZeroLockTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Must match the installed engine's shared build environment (same as ZeroLockEditor.Target.cs), or packaging fails
		// with "modifies the values of properties ... has build products in common with UnrealGame".
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		// Runtime only: ZeroEditorModule (Editor) and ZeroLockUncooked (UncookedOnly) never go into a cooked game.
		ExtraModuleNames.Add("ZeroLock");
	}
}
