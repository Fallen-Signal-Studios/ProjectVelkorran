// Copyright Fallen Signal Studios. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class ProjectVelkorranTests : ModuleRules
{
    public ProjectVelkorranTests(ReadOnlyTargetRules Target) : base(Target)
    {
        if (Target.Type != TargetType.Editor)
        {
            throw new BuildException("ProjectVelkorranTests contains reflected fixtures and may only build for Editor targets.");
        }
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Unity is deliberately left at the engine default. Disabling it hides cross-translation-unit
        // symbol collisions in this module instead of preventing them, and a defect class that only
        // appears when the adaptive grouping happens to shift is a hole in what the suite can qualify.
        // Collisions are fixed at the root here, as they were in ProjectVelkorranEditor.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "ProjectVelkorran", "NarrativeArsenal", "NarrativeSaveSystem",
            "NarrativeCommonUI", "GameplayAbilities", "GameplayTags", "GameplayTasks", "AIModule",
            "UMG", "CommonUI", "CommonInput", "Slate", "SlateCore", "InputCore", "ApplicationCore", "EnhancedInput",
            "PhysicsCore", "Niagara", "NavigationSystem", "AssetRegistry", "DeveloperSettings",
            "LevelSequence", "MovieScene", "MovieSceneTracks", "AnimGraphRuntime",
            "MassEntity", "MassCommon", "MassActors", "MassSpawner", "MassRepresentation", "MassCrowd",
            "OnlineSubsystem", "OnlineSubsystemUtils"
        });
    }
}
