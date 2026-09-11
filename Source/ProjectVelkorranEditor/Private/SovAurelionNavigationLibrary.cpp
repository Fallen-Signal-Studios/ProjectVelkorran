// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionNavigationLibrary.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "UObject/Package.h"

namespace
{
    UNavigationSystemV1* GetOwnedQueryNavigation(UWorld* World)
    {
        if (!IsInGameThread() || !GEditor || !IsValid(World) || World->IsTemplate()) { return nullptr; }
        const bool bCurrentEditor = World->WorldType == EWorldType::Editor
            && GEditor->GetEditorWorldContext().World() == World;
        const bool bCurrentPIE = World->WorldType == EWorldType::PIE && GEditor->PlayWorld == World
            && !GEditor->IsSimulateInEditorInProgress();
        const FString Package = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
        if ((!bCurrentEditor && !bCurrentPIE)
            || (Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12") && Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M13")))
        { return nullptr; }
        auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        return IsValid(Navigation) && !Navigation->IsTemplate() && Navigation->GetWorld() == World
            ? Navigation : nullptr;
    }
}

bool USovAurelionNavigationLibrary::IsNavigationBeingBuiltOrLocked(UWorld* World)
{
    // Call the unchanged native query directly, without ProcessEvent on its Within=World CDO.
    return !GetOwnedQueryNavigation(World) || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World);
}

UNavigationPath* USovAurelionNavigationLibrary::FindPathToLocationSynchronously(
    UWorld* World, FVector PathStart, FVector PathEnd, AActor* PathfindingContext,
    TSubclassOf<UNavigationQueryFilter> FilterClass)
{
    if (!GetOwnedQueryNavigation(World) || PathStart.ContainsNaN() || PathEnd.ContainsNaN()
        || (PathfindingContext && (!IsValid(PathfindingContext) || PathfindingContext->IsTemplate()
            || PathfindingContext->GetWorld() != World || PathfindingContext->IsActorBeingDestroyed())))
    { return nullptr; }
    // Direct C++ invocation uses the supplied current world; it does not ProcessEvent on the nav CDO.
    return UNavigationSystemV1::FindPathToLocationSynchronously(World, PathStart, PathEnd, PathfindingContext, FilterClass);
}

ANavMeshBoundsVolume* USovAurelionNavigationLibrary::BuildNavigation(UWorld* World, FVector Center, FVector Extent, FString& Error)
{
    Error.Reset();
    const FString Path = World ? World->GetOutermost()->GetName() : FString();
    if (!GEditor || GEditor->PlayWorld || !World || World != GEditor->GetEditorWorldContext().World()
        || (Path != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12") && Path != TEXT("/Game/Aurelion/Maps/L_Aurelion_M13"))
        || Center.ContainsNaN() || Extent.ContainsNaN() || Extent.GetMin() < 100 || Extent.GetMax() > 100000)
    { Error = TEXT("Navigation authoring requires a stopped owned Aurelion map and finite bounded volume."); return nullptr; }
    auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Navigation) { Error = TEXT("This map has no configured Navigation system."); return nullptr; }
    ANavMeshBoundsVolume* Volume = nullptr;
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
    {
        if (It->GetFName() == TEXT("AurelionAuthoredNavigation"))
        { if (Volume) { Error = TEXT("Ambiguous authored navigation volume."); return nullptr; } Volume = *It; }
    }
    if (!Volume)
    {
        FActorSpawnParameters Params; Params.Name = TEXT("AurelionAuthoredNavigation"); Params.ObjectFlags |= RF_Transactional;
        Volume = World->SpawnActor<ANavMeshBoundsVolume>(Center, FRotator::ZeroRotator, Params);
    }
    if (!Volume) { Error = TEXT("Could not create the navigation volume."); return nullptr; }
    Volume->SetActorLocation(Center);
    Volume->SetActorLabel(TEXT("Aurelion_Navigation"));
    Volume->SetFolderPath(TEXT("Aurelion/Runtime/Navigation"));
    UCubeBuilder* Builder = NewObject<UCubeBuilder>();
    Builder->X = Extent.X * 2; Builder->Y = Extent.Y * 2; Builder->Z = Extent.Z * 2;
    UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
    Volume->SetActorHiddenInGame(true);
    Navigation->OnNavigationBoundsUpdated(Volume);
    Navigation->Build();
    return Volume;
}
