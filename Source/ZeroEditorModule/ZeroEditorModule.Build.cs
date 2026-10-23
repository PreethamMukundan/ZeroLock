using UnrealBuildTool;

public class ZeroEditorModule : ModuleRules
{
    public ZeroEditorModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", 
                "ScriptableEditorWidgets"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "ZeroLock",
                "GameplayAbilities",
                "GameplayTags",
                "Blutility",
                "UMG",
                "UMGEditor",
                "UnrealEd",
                "LevelEditor",
                "PropertyEditor",
                "AssetRegistry",
                "EngineSettings",
                "InputCore",
                "AssetTools",
                "Kismet",
                "CurveTableEditor", 
            }
        );
    }
}