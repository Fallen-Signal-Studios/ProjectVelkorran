// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovAurelionDeparturePresentation.generated.h"

/** Reads the completed native M13 journal, then preserves the companion's physical
 * departure through its ordinary hold command. No receipt, teleport or save write. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionDeparturePresentation : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionDeparturePresentation();
    virtual void Tick(float DeltaSeconds) override;
};
