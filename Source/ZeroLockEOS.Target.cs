// Copyright Preetham Mukundan (C) 2026

using UnrealBuildTool;
using System.Collections.Generic;

// Game target that layers Config/Custom/EOS on top of the default config (Lyra's LyraGameEOS).
// Staging picks up Config/Custom/EOS from this target. On the installed (launcher) engine the build environment is
// shared, so CUSTOM_CONFIG is not compiled in: launch the packaged game with -CustomConfig=EOS (non-Shipping only).
public class ZeroLockEOSTarget : ZeroLockTarget
{
	public ZeroLockEOSTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "EOS";
	}
}
