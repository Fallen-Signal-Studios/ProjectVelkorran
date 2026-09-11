// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionCrossfireAuthoringLibrary.h"
#include "SovAurelionCrossfireInspectionLibrary.generated.h"
class ANarrativeNPCController;

USTRUCT(BlueprintType)
struct FSovCrossfireQueryReadback
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 QueryId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FString QueryName;
    UPROPERTY(BlueprintReadOnly) FString QueryOwner;
    UPROPERTY(BlueprintReadOnly) double CompletedAt = 0.;
    UPROPERTY(BlueprintReadOnly) double ExecutionMilliseconds = 0.;
    UPROPERTY(BlueprintReadOnly) bool bFinished = false;
    UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly) bool bAborted = false;
    UPROPERTY(BlueprintReadOnly) int32 NativeStatus = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FVector> ResultLocations;
    UPROPERTY(BlueprintReadOnly) TArray<float> ResultScores;
};

USTRUCT(BlueprintType)
struct FSovCrossfireRuntimeReadback
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bCurrentOwner = false;
    UPROPERTY(BlueprintReadOnly) bool bQueryHistoryAvailable = false;
    UPROPERTY(BlueprintReadOnly) FString UnavailableReason;
    UPROPERTY(BlueprintReadOnly) FString Pawn;
    UPROPERTY(BlueprintReadOnly) FString Controller;
    UPROPERTY(BlueprintReadOnly) FString AttackTarget;
    UPROPERTY(BlueprintReadOnly) FString Activity;
    UPROPERTY(BlueprintReadOnly) FString Goal;
    UPROPERTY(BlueprintReadOnly) FString Encounter;
    UPROPERTY(BlueprintReadOnly) FGuid Attempt;
    UPROPERTY(BlueprintReadOnly) double GameSeconds = 0.;
    UPROPERTY(BlueprintReadOnly) FVector PawnLocation = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bHasBlackboardDestination = false;
    UPROPERTY(BlueprintReadOnly) FVector BlackboardDestination = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) int64 MoveRequestId = 0;
    UPROPERTY(BlueprintReadOnly) int32 MoveStatus = 0;
    UPROPERTY(BlueprintReadOnly) FVector PathDestination = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) TArray<FSovCrossfireQueryReadback> Queries;
};

/** Reads only existing serialized assets or native debugger/path state. Never starts a query. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionCrossfireInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Aurelion|Inspection")
    static FSovAurelionCrossfireAuthoringResult InspectSavedCrossfire();
    UFUNCTION(BlueprintPure, Category="Aurelion|Inspection")
    static FSovCrossfireRuntimeReadback ReadCurrentCrossfire(ANarrativeNPCController* Controller);
};
