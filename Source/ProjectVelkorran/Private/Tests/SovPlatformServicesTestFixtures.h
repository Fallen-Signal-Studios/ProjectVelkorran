// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Platform/SovPlatformServicesSubsystem.h"
#include "SovPlatformServicesTestFixtures.generated.h"

UCLASS()
class USovCloudReentryProbe : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<USovPlatformServicesSubsystem> Service;
    bool bCancelReading = false;
    bool bCancelWriting = false;
    bool bRestartOnComplete = false;
    bool bRestartOnCancel = false;
    bool bDeinitializeOnReading = false;
    bool bRestartAccepted = false;
    UFUNCTION() void OnChanged(const FSovCloudReview& Review)
    {
        if (!Service) { return; }
        if (Review.Phase == ESovCloudPhase::Reading && bDeinitializeOnReading)
        { bDeinitializeOnReading = false; Service->Deinitialize(); return; }
        if (Review.Phase == ESovCloudPhase::Reading && bCancelReading)
        { bCancelReading = false; Service->CancelCloudOperation(); return; }
        if (Review.Phase == ESovCloudPhase::Writing && bCancelWriting)
        { bCancelWriting = false; Service->CancelCloudOperation(); return; }
        if ((Review.Phase == ESovCloudPhase::Completed && bRestartOnComplete)
            || (Review.Phase == ESovCloudPhase::Cancelled && bRestartOnCancel))
        {
            bRestartOnComplete = false; bRestartOnCancel = false; FString Error;
            bRestartAccepted = Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error);
        }
    }
};
