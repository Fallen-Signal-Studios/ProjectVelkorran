using UnrealBuildTool;

public class ProjectVelkorranAnimGraph : ModuleRules
{
    public ProjectVelkorranAnimGraph(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AnimGraph", "BlueprintGraph", "ProjectVelkorran" });
    }
}
