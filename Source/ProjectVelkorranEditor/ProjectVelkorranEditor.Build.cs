// Copyright Fallen Signal Studios. All Rights Reserved.
using UnrealBuildTool;

public class ProjectVelkorranEditor : ModuleRules
{
    public ProjectVelkorranEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        if (Target.Type != TargetType.Editor)
        {
            throw new BuildException("ProjectVelkorranEditor is an asset authoring module for Editor targets only.");
        }
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] { "ProjectVelkorran", "AIModule", "GameplayAbilities", "GameplayTags", "AssetRegistry", "UnrealEd", "Kismet", "KismetCompiler", "BlueprintGraph", "UMG", "UMGEditor", "LevelSequence", "MovieScene", "NavigationSystem", "NarrativeArsenal", "NarrativeSaveSystem", "InputCore", "ApplicationCore", "Niagara" });
    }
}
