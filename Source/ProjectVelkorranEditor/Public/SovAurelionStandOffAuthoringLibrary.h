// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionStandOffAuthoringLibrary.generated.h"
class UEnvQuery;
class UBehaviorTree;

USTRUCT(BlueprintType)
struct FSovAurelionStandOffInspection
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly) bool bInstalled = false;
    UPROPERTY(BlueprintReadOnly) FString Error;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UEnvQuery> Query;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UBehaviorTree> Tree;
    UPROPERTY(BlueprintReadOnly) FString SourceQuerySnapshot;
    UPROPERTY(BlueprintReadOnly) FString QuerySnapshot;
    UPROPERTY(BlueprintReadOnly) FString QueryPreservedSnapshot;
    UPROPERTY(BlueprintReadOnly) FString TreeSnapshot;
    UPROPERTY(BlueprintReadOnly) FString TreePreservedSnapshot;
    UPROPERTY(BlueprintReadOnly) int32 CandidatesPerContext = 0;
    UPROPERTY(BlueprintReadOnly) float RadiusCm = 0.f;
    UPROPERTY(BlueprintReadOnly) float SpacingCm = 0.f;
    UPROPERTY(BlueprintReadOnly) float ArcDegrees = 0.f;
};

/** Exact owned asset operation only; never saves, executes a query, spawns or enters PIE. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionStandOffAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Aurelion|Authoring")
    static FSovAurelionStandOffInspection InspectStandOff(bool bRequireInstalled);
    UFUNCTION(BlueprintCallable, Category="Aurelion|Authoring")
    static FSovAurelionStandOffInspection AuthorStandOff();
    UFUNCTION(BlueprintCallable, Category="Aurelion|Authoring")
    static FSovAurelionStandOffInspection RestoreStandOffBinding(const FString& ExpectedOriginalTreeSnapshot);
};
