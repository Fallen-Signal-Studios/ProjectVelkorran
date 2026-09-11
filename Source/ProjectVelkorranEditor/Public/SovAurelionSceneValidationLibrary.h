// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionSceneValidationLibrary.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct FSovAurelionExitGeometryResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly) bool bClear = false;
    UPROPERTY(BlueprintReadOnly) bool bRecoveryExcluded = false;
    UPROPERTY(BlueprintReadOnly) FString Report;
    UPROPERTY(BlueprintReadOnly) TArray<FString> BlockingComponents;
    UPROPERTY(BlueprintReadOnly) TArray<FString> IgnoredCharacters;
};

/** Read-only authoring checks; no object, asset, actor or collision-state mutation. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionSceneValidationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Exact native ECC_Pawn capsule query in either owned stopped wrapper.
     * Characters and their attachments are excluded because the authoring caller
     * checks their logical scene positions separately. Gates and requests remain.
     */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovAurelionExitGeometryResult ValidateAurelionExitGeometry(
        UWorld* World, FTransform ExitTransform, float Radius, float HalfHeight);
};
