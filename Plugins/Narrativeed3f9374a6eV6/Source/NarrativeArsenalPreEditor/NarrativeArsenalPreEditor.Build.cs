using UnrealBuildTool;

public class NarrativeArsenalPreEditor : ModuleRules
{
    public NarrativeArsenalPreEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "UnrealEd",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",
                "NarrativeSaveSystem", 
                "NarrativeArsenal", 
                "GraphEditor",
                "KismetWidgets",
                "PropertyEditor"
            }
        );
    }
}