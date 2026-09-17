// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ZeroLockEditorTarget : TargetRules
{
	public ZeroLockEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7; 
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8; 
		ExtraModuleNames.Add("ZeroLock");
		RegisterModulesCreatedByRider();
	}

	private void RegisterModulesCreatedByRider()
	{
		ExtraModuleNames.AddRange(new string[] { "ZeroEditorModule" });
	}
}
