// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Cinematics/SovAurelionStorySequenceActor.h"
#include "SovAurelionRequestRuntimeTestFixtures.generated.h"
UCLASS()
class ASovAurelionStoryTestActor : public ASovAurelionStorySequenceActor
{
    GENERATED_BODY()
public:
    ASovAurelionStoryTestActor(const FObjectInitializer& Initializer) : Super(Initializer) {}
    void InitializeTestSequence(ULevelSequence* Sequence) { SetSequence(Sequence); InitializePlayer(); }
};
