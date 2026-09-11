// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionCrossfireAuthoringLibrary.generated.h"
class UBehaviorTree;
class UEnvQuery;
class UBlueprint;

USTRUCT(BlueprintType)
struct FSovAurelionCrossfireAuthoringResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly) FString Error;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UEnvQuery> Query;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UBehaviorTree> Tree;
    UPROPERTY(BlueprintReadOnly) FString SourceRuntimeNodes;
    UPROPERTY(BlueprintReadOnly) FString PreservedRuntimeNodes;
};

/** Explicit editor-only authoring. Does not save, spawn an NPC or change a map. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionCrossfireAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Aurelion|Authoring")
    static FSovAurelionCrossfireAuthoringResult CreateCrossfireAssets();
    UFUNCTION(BlueprintCallable, Category="Aurelion|Authoring")
    static bool BindCrossfireActivity(UBlueprint* OwnedActivity, UBehaviorTree* Tree);
    UFUNCTION(BlueprintCallable, Category="Aurelion|Authoring")
    static bool InstallCrossfireActivity(UBlueprint* OwnedActivity);
};
