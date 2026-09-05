// Copyright Fallen Signal Studios. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

/** Native fixtures and automation are editor-only; Shipping never reflects them. */
public class ProjectVelkorranTests : ModuleRules
{
    public ProjectVelkorranTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "ProjectVelkorran",
            "NarrativeArsenal", "NarrativeSaveSystem", "NarrativeCommonUI",
            "AIModule", "ApplicationCore", "InputCore", "EnhancedInput",
            "GameplayAbilities", "GameplayTags", "GameplayTasks", "PhysicsCore", "Niagara",
            "UMG", "CommonUI", "CommonInput", "Slate", "SlateCore",
            "MassEntity", "MassCommon", "MassActors", "MassSpawner", "MassRepresentation", "MassCrowd",
            "AnimGraphRuntime", "NavigationSystem", "AssetRegistry", "LevelSequence", "MovieScene", "MovieSceneTracks",
            "OnlineSubsystem", "OnlineSubsystemUtils"
        });
        // Tests deliberately exercise production policies/adapters through their
        // existing owners. This path does not create a runtime -> test dependency.
        PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ProjectVelkorran/Private")));
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("TextToSpeech");
            PrivateDefinitions.Add("SOV_WITH_TEXT_TO_SPEECH=1");
        }
        else { PrivateDefinitions.Add("SOV_WITH_TEXT_TO_SPEECH=0"); }
    }
}
