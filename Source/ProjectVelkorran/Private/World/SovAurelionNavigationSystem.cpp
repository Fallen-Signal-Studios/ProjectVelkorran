// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovAurelionNavigationSystem.h"

void ASovAurelionRecastNavMesh::PostInitProperties()
{
    Super::PostInitProperties();
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        // PostInit runs after inherited Engine config loading. Only this owned class CDO changes.
        AgentRadius = USovAurelionNavigationSystem::ProfileRadius;
        AgentHeight = USovAurelionNavigationSystem::ProfileHeight;
        RuntimeGeneration = ERuntimeGenerationType::Dynamic;
    }
}

FNavDataConfig USovAurelionNavigationSystem::MakeAgentConfig()
{
    FNavDataConfig Agent(ProfileRadius, ProfileHeight);
    Agent.Name = TEXT("AurelionHumanoid");
    Agent.Color = FColor(72, 196, 235);
    Agent.DefaultQueryExtent = FVector(160.f, 160.f, 350.f);
    Agent.SetNavDataClass(ASovAurelionRecastNavMesh::StaticClass());
    return Agent;
}

void USovAurelionNavigationSystem::PostInitProperties()
{
    Super::PostInitProperties();
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        // Later Configure/ApplySupportedAgentsFilter restore this owned profile, not the stock radius2 fallback.
        SupportedAgents = { MakeAgentConfig() };
        SupportedAgentsMask.Empty();
        SupportedAgentsMask.Set(0);
        SupportedAgentsMask.MarkInitialized();
        DefaultAgentName = SupportedAgents[0].Name;
    }
}

USovAurelionNavigationConfig::USovAurelionNavigationConfig(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    NavigationSystemClass = FSoftClassPath(USovAurelionNavigationSystem::StaticClass());
    SupportedAgentsMask.Empty();
    SupportedAgentsMask.Set(0);
    SupportedAgentsMask.MarkInitialized();
    DefaultAgentName = TEXT("AurelionHumanoid");
    bStrictlyStatic = false;
    bCreateOnClient = false;
    bAutoSpawnMissingNavData = true;
    bSpawnNavDataInNavBoundsLevel = false;
}
