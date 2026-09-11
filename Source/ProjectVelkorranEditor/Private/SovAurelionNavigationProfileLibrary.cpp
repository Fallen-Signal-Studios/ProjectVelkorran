// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionNavigationProfileLibrary.h"
#include "World/SovAurelionNavigationSystem.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

FSovAurelionNavigationInspection USovAurelionNavigationProfileLibrary::InspectNavigation(UWorld* World)
{
    FSovAurelionNavigationInspection Result;
    auto* Settings = World ? World->GetWorldSettings() : nullptr;
    auto* Config = Settings ? Settings->GetNavigationSystemConfig() : nullptr;
    auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    Result.ConfigClass = GetPathNameSafe(Config ? Config->GetClass() : nullptr);
    Result.ConfigOuter = GetPathNameSafe(Config ? Config->GetOuter() : nullptr);
    Result.SystemClass = GetPathNameSafe(Navigation ? Navigation->GetClass() : nullptr);
    if (!Settings || !Config || !Navigation || Config->GetClass() != USovAurelionNavigationConfig::StaticClass()
        || Config->GetOuter() != Settings || Settings->GetNavigationSystemConfigOverride()
        || Navigation->GetClass() != USovAurelionNavigationSystem::StaticClass())
    { Result.Error = TEXT("World must own the saved Aurelion config and its current native navigation system."); return Result; }
    const auto& Agents = Navigation->GetSupportedAgents();
    const FNavDataConfig Expected = USovAurelionNavigationSystem::MakeAgentConfig();
    if (Agents.Num() != 1 || !Navigation->GetSupportedAgentsMask().Contains(0)
        || !Agents[0].IsEquivalent(Expected) || Agents[0].Name != Expected.Name
        || Agents[0].GetNavDataClass<ANavigationData>() != ASovAurelionRecastNavMesh::StaticClass())
    { Result.Error = TEXT("The current supported-agent profile does not match the actual Aurelion capsule contract."); return Result; }
    Result.bConfigured = true;
    Result.AgentRadius = Agents[0].AgentRadius;
    Result.AgentHeight = Agents[0].AgentHeight;
    bool bAllDataMatches = true;
    for (ANavigationData* Data : Navigation->NavDataSet)
    {
        if (!IsValid(Data)) { bAllDataMatches = false; continue; }
        Result.RegisteredNavData.Add(Data->GetPathName());
        const bool bMatches = Data->GetClass() == ASovAurelionRecastNavMesh::StaticClass()
            && Data->IsRegistered() && Data->GetConfig().IsEquivalent(Expected);
        const bool bDynamicData = Data->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic;
        bAllDataMatches &= bMatches && bDynamicData;
        Result.bDynamic |= bMatches && bDynamicData;
    }
    Result.bRegistered = bAllDataMatches && Result.RegisteredNavData.Num() == 1 && Result.bDynamic;
    if (!Result.bRegistered) { Result.Error = TEXT("Await the native build: exactly one matching dynamic navigation data actor must register."); }
    return Result;
}

FSovAurelionNavigationInspection USovAurelionNavigationProfileLibrary::ConfigureNavigation(
    UWorld* World, float RequiredRadius, float RequiredHeight)
{
    FSovAurelionNavigationInspection Failure;
    const FString Path = World ? World->GetOutermost()->GetName() : FString();
    if (!GEditor || GEditor->PlayWorld || !World || World != GEditor->GetEditorWorldContext().World()
        || (Path != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12") && Path != TEXT("/Game/Aurelion/Maps/L_Aurelion_M13")))
    { Failure.Error = TEXT("Configuration authoring requires the stopped owned Aurelion M12 or M13 map."); return Failure; }
    if (!FMath::IsFinite(RequiredRadius) || !FMath::IsFinite(RequiredHeight) || RequiredRadius <= 0.f
        || RequiredHeight < RequiredRadius * 2.f
        || RequiredRadius > USovAurelionNavigationSystem::ProfileRadius + UE_KINDA_SMALL_NUMBER
        || RequiredHeight > USovAurelionNavigationSystem::ProfileHeight + UE_KINDA_SMALL_NUMBER)
    { Failure.Error = TEXT("Actual authored capsule maxima exceed or invalidate the native Aurelion navigation profile."); return Failure; }
    auto* Settings = World->GetWorldSettings();
    auto* Property = FindFProperty<FObjectPropertyBase>(AWorldSettings::StaticClass(), TEXT("NavigationSystemConfig"));
    if (!Settings || !Property || !Property->HasAnyPropertyFlags(CPF_InstancedReference)
        || Settings->GetNavigationSystemConfigOverride())
    { Failure.Error = TEXT("Saved instanced navigation config is unavailable or overridden by another owner."); return Failure; }
    auto* Existing = Settings->GetNavigationSystemConfig();
    if (Existing && Existing->GetClass() != UNavigationSystemConfig::StaticClass()
        && Existing->GetClass() != UNavigationSystemModuleConfig::StaticClass()
        && Existing->GetClass() != USovAurelionNavigationConfig::StaticClass())
    { Failure.Error = TEXT("Refusing to replace an unrelated custom navigation config."); return Failure; }
    const FSovAurelionNavigationInspection Before = InspectNavigation(World);
    if (Before.bConfigured) { return Before; }
    auto* Config = Cast<USovAurelionNavigationConfig>(Existing);
    if (!Config || Config->GetOuter() != Settings)
    { Config = NewObject<USovAurelionNavigationConfig>(Settings, NAME_None, RF_Transactional); }
    Settings->Modify();
    Settings->PreEditChange(Property);
    Property->SetObjectPropertyValue_InContainer(Settings, Config);
    FPropertyChangedEvent Changed(Property, EPropertyChangeType::ValueSet);
    // Engine WorldSettings handles cleanup + recreation before registration, exactly as the Details UI does.
    Settings->PostEditChangeProperty(Changed);
    return InspectNavigation(World);
}
