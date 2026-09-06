// Copyright Fallen Signal Studios. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class ProjectVelkorranTests : ModuleRules
{
    public ProjectVelkorranTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Keep the regression translation units independent. Their reflected
        // fixtures must never enter a Game/Shipping target through a dependency.
        bUseUnity = false;
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "ProjectVelkorran",
            "NarrativeArsenal", "NarrativeSaveSystem", "NarrativeCommonUI",
            "AIModule", "InputCore", "ApplicationCore", "EnhancedInput",
            "GameplayAbilities", "GameplayTags", "GameplayTasks", "Niagara",
            "PhysicsCore", "NavigationSystem", "AnimGraphRuntime",
            "UMG", "CommonUI", "Slate", "SlateCore",
            "LevelSequence", "MovieScene", "MovieSceneTracks",
            "MassEntity", "MassCommon", "MassActors", "MassSpawner",
            "MassRepresentation", "MassCrowd"
        });
        // The platform test double and two inline policies intentionally remain
        // implementation details of the runtime module. No runtime .cpp is reused.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "../ProjectVelkorran/Private"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
    }
}
