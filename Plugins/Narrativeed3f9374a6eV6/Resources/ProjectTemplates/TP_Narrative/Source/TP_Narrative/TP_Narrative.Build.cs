// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TP_Narrative : ModuleRules
{
	public TP_Narrative(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
                "Core",
                "NarrativeSaveSystem",
                "LevelSequence",
                "MovieScene",
                "CinematicCamera",
                "AIModule",
                "HairStrandsCore",
                "MediaAssets",
                "UMG",
                "PhysicsCore",
                "GameplayAbilities",
                "GameplayCameras",
                "CommonUI",
                "NavigationSystem",
                "ZoneGraph",
                "MassActors",
                "MassSpawner",
                "MassCrowd",
                "MassRepresentation",
                "GameplayTasks",
                "PoseSearch",
                "Chooser",
                "ZoneGraphAnnotations",
                "EngineSettings",
                "GameplayTags"
        });

		PrivateDependencyModuleNames.AddRange(new string[] {});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
