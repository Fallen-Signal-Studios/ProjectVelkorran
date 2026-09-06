// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Save/SovCampaignSaveGame.h"
#include "NarrativeSave.h"
#include "SovSaveOperationTestFixtures.generated.h"

/** Actual envelope serialization callback, not a replacement for the production serializer. */
UCLASS()
class USovSaveOperationEnvelope : public USovCampaignSaveGame
{
    GENERATED_BODY()
public:
    static TFunction<void(bool)> SerializationBoundary;
    virtual void Serialize(FArchive& Ar) override;
};

UCLASS()
class USovSaveOperationNarrative : public UNarrativeSave
{
    GENERATED_BODY()
public:
    static TFunction<void()> LoadingBoundary;
    virtual void Serialize(FArchive& Ar) override;
};

UCLASS()
class USovSaveOperationCompletion : public UObject
{
    GENERATED_BODY()
public:
    int32 Notifications = 0;
    ESovSaveResult Result = ESovSaveResult::MissingSave;
    UFUNCTION() void Completed(ESovSaveResult InResult, const FSovSaveSlotHeader& Header, const FString& Error)
    { ++Notifications; Result = InResult; }
};
