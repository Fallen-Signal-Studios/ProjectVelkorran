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
				"InputCore",
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"NarrativeArsenal",
				"NarrativeSaveSystem"
			}
		);
	}
}
