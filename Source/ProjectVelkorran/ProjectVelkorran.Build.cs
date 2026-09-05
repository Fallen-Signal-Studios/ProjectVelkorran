// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectVelkorran : ModuleRules
{
	public ProjectVelkorran(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AIModule",
				"InputCore",
				"ApplicationCore",
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"Niagara",
				"PhysicsCore",
				"NarrativeArsenal",
				"NarrativeSaveSystem",
				"NarrativeCommonUI",
				"UMG",
				"CommonUI",
				"MassEntity",
				"MassCommon",
				"MassActors",
				"MassSpawner",
				"MassRepresentation",
				"MassCrowd"
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraphRuntime", "NavigationSystem", "AssetRegistry", "LevelSequence", "MovieScene", "MovieSceneTracks", "Slate", "SlateCore", "OnlineSubsystem", "OnlineSubsystemUtils" });
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("TextToSpeech");
			PrivateDefinitions.Add("SOV_WITH_TEXT_TO_SPEECH=1");
		}
		else { PrivateDefinitions.Add("SOV_WITH_TEXT_TO_SPEECH=0"); }
	}
}
