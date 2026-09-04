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
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"Niagara",
				"PhysicsCore",
				"NarrativeArsenal",
				"NarrativeSaveSystem"
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraphRuntime", "NavigationSystem" });
	}
}
