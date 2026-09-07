// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NarrativeArsenal : ModuleRules
{
	public NarrativeArsenal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
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
                "GameplayTags",
                "NetCore",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
                "Json",
				"Engine",
				"Slate",
				"SlateCore",
				"AIModule",
                "GameplayAbilities",
                "GameplayCameras",
                "GameplayTags",
				"MotionWarping",
                "EnhancedInput",
				"NarrativePro",
				"NarrativeCommonUI",
				"MovieSceneTracks",
				// ... add private dependencies that you statically link with here ...	
				
				"HairStrandsCore",
				"Niagara",
                "CommonInput",
				"DeveloperSettings",
				"InputCore", 
				"ZoneGraph", 
				"MassZoneGraphNavigation",
				"MassCommon",
				"MassEntity",
				"MassMovement",
				"MassNavigation", 
				"MovieScene",
				"ChaosVehicles",
				"MassGameplayExternalTraits",
				"MassLOD",
				"MassSimulation",
				"Navmesh", "MassGameplayDebug"
			}
			);

        PublicIncludePathModuleNames.AddRange(new string[] { "RHI" });

        if ((Target.IsInPlatformGroup(UnrealPlatformGroup.Windows)))
        {
            // Uses DXGI to query GPU hardware
            // This is what will allow us to get GPU usage statistics at runtime
            PublicSystemLibraries.Add("DXGI.lib");
        }

        DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		SetupGameplayDebuggerSupport(Target, true);
		
		// Editor tools
		if (Target.Type == TargetType.Editor)
		{
			PublicDependencyModuleNames.Add("SubobjectDataInterface");
			PublicDependencyModuleNames.Add("UnrealEd");
		}
	}
}
