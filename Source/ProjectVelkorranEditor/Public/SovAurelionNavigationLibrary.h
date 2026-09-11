// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionNavigationLibrary.generated.h"
class ANavMeshBoundsVolume;
class AActor;
class UWorld;
class UNavigationPath;
class UNavigationQueryFilter;
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionNavigationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Creates a real editor brush, updates Navigation's bounds, and starts the normal native build. Never saves. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static ANavMeshBoundsVolume* BuildNavigation(UWorld* World, FVector Center, FVector Extent, FString& Error);

    /** Read-only native query for the current owned Editor/PIE world. Invalid context or missing
     * navigation fails closed as pending. No CDO dispatch into NavigationSystemV1 occurs. */
    UFUNCTION(BlueprintPure, Category="Aurelion|Editor|Navigation Query")
    static bool IsNavigationBeingBuiltOrLocked(UWorld* World);

    /** Ordinary native path query; returns null on invalid scope/coordinates/context.
     * This does not request movement, rebuild navigation, or alter world configuration. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor|Navigation Query")
    static UNavigationPath* FindPathToLocationSynchronously(UWorld* World, FVector PathStart, FVector PathEnd,
        AActor* PathfindingContext = nullptr, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);
};
