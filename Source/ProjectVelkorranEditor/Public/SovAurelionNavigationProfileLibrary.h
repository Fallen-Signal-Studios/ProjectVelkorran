// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionNavigationProfileLibrary.generated.h"

USTRUCT(BlueprintType)
struct PROJECTVELKORRANEDITOR_API FSovAurelionNavigationInspection
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bConfigured = false;
    UPROPERTY(BlueprintReadOnly) bool bRegistered = false;
    UPROPERTY(BlueprintReadOnly) bool bDynamic = false;
    UPROPERTY(BlueprintReadOnly) FString Error;
    UPROPERTY(BlueprintReadOnly) FString ConfigClass;
    UPROPERTY(BlueprintReadOnly) FString ConfigOuter;
    UPROPERTY(BlueprintReadOnly) FString SystemClass;
    UPROPERTY(BlueprintReadOnly) float AgentRadius = 0.f;
    UPROPERTY(BlueprintReadOnly) float AgentHeight = 0.f;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RegisteredNavData;
};

/** Saved map configuration only; never edits project settings or stock class defaults. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionNavigationProfileLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Validates actual authored capsule maxima and changes only an owned, stopped map's instanced config.
     * Uses WorldSettings' normal property-change lifecycle. Never saves or builds. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static FSovAurelionNavigationInspection ConfigureNavigation(UWorld* World, float RequiredRadius, float RequiredHeight);
    /** Read-only inspection is valid in an opened editor world and its actual PIE world. */
    UFUNCTION(BlueprintPure, Category="Aurelion|Editor")
    static FSovAurelionNavigationInspection InspectNavigation(UWorld* World);
};
