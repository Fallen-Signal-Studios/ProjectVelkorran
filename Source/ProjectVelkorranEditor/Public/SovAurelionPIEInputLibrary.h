// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionPIEInputLibrary.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct FSovAurelionPIEMouseInputResult
{
    GENERATED_BODY()
    /** Both synthetic axis samples reached the same validated player input owner. */
    UPROPERTY(BlueprintReadOnly) bool bRouted = false;
    /** Unreal normally returns false for analog samples despite accumulating them. */
    UPROPERTY(BlueprintReadOnly) bool bMouseXConsumed = false;
    UPROPERTY(BlueprintReadOnly) bool bMouseYConsumed = false;
    UPROPERTY(BlueprintReadOnly) FString ControllerPath;
    UPROPERTY(BlueprintReadOnly) FString Report;
};

/** Editor-only synthetic mouse input for the two exact Aurelion PIE wrappers. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionPIEInputLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Routes one MouseX/MouseY sample through the ordinary player-controller input path.
     * Each finite delta must be within +/-512. Normal subsequent input processing,
     * mouse settings and the actual menu determine the effect; no UI or equipment
     * state is set. This does not validate physical desktop mouse delivery.
     */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|PIE Input")
    static FSovAurelionPIEMouseInputResult InjectAurelionPIEMouseDelta(
        UWorld* World, float DeltaX, float DeltaY);
};
