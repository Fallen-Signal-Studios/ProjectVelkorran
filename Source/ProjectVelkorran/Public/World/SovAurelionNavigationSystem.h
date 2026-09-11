// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "SovAurelionNavigationSystem.generated.h"

/** Native defaults belong only to worlds opting into the saved Aurelion config. */
UCLASS(NotBlueprintable, NotPlaceable)
class PROJECTVELKORRAN_API ASovAurelionRecastNavMesh : public ARecastNavMesh
{
    GENERATED_BODY()
public:
    virtual void PostInitProperties() override;
};

/** A dedicated agent universe is required: UE's OverrideSupportedAgents only filters its class CDO.
 * The stock navigation CDO and project SupportedAgents remain unchanged. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovAurelionNavigationSystem : public UNavigationSystemV1
{
    GENERATED_BODY()
public:
    static constexpr float ProfileRadius = 34.f;
    static constexpr float ProfileHeight = 176.f;
    static FNavDataConfig MakeAgentConfig();
    virtual void PostInitProperties() override;
private:
    friend struct FSovAurelionNavigationTestAccess;
};

/** Saved as WorldSettings' instanced config; selected before nav-data registration in Editor/PIE/Game. */
UCLASS(EditInlineNew, NotBlueprintable, DisplayName="Aurelion Navigation Config")
class PROJECTVELKORRAN_API USovAurelionNavigationConfig : public UNavigationSystemModuleConfig
{
    GENERATED_BODY()
public:
    USovAurelionNavigationConfig(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
