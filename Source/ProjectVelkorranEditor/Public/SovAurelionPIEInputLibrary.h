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

USTRUCT(BlueprintType)
struct FSovAurelionPIEPointerInputResult
{
    GENERATED_BODY()
    /** Press and release both reached Slate for the same validated PIE viewport. */
    UPROPERTY(BlueprintReadOnly) bool bRouted = false;
    UPROPERTY(BlueprintReadOnly) bool bPressHandled = false;
    UPROPERTY(BlueprintReadOnly) bool bReleaseHandled = false;
    UPROPERTY(BlueprintReadOnly) FVector2D ScreenPosition = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FString Report;
};

USTRUCT(BlueprintType)
struct FSovAurelionPIEViewportCaptureResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bCaptured = false;
    UPROPERTY(BlueprintReadOnly) FString Filename;
    UPROPERTY(BlueprintReadOnly) int32 Width = 0;
    UPROPERTY(BlueprintReadOnly) int32 Height = 0;
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

    /** Routes one left-button press and release through Slate at the PIE viewport centre.
     * This is the same application input path a desktop click takes, including input
     * preprocessors and CommonUI action routing; no widget handler or equipment state is
     * called directly. It does not validate physical desktop mouse delivery.
     */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|PIE Input")
    static FSovAurelionPIEPointerInputResult InjectAurelionPIELeftClick(UWorld* World);

    /** Save the sole local player's current PIE back buffer under Saved/Validation/Aurelion.
     * This reads the actual game viewport even when an editor level viewport owns focus.
     */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|PIE Validation")
    static FSovAurelionPIEViewportCaptureResult CaptureAurelionPIEViewport(
        UWorld* World, const FString& Filename);
};
