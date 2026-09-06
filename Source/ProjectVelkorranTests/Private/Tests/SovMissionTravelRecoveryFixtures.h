// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Save/SovSaveSubsystem.h"
#include "SovMissionTravelRecoveryFixtures.generated.h"

UCLASS()
class USovMissionTravelTestSubsystem : public USovSaveSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject*) const override { return false; }
    virtual UWorld* GetWorld() const override { return TestWorld.Get(); }
    TWeakObjectPtr<UWorld> TestWorld;
    int32 RecoveryRequests = 0;
    FString RequestedMap;
    FString RequestedOptions;
protected:
    virtual void StartMissionRecoveryTravel(UWorld&, const FString& Map, const FString& Options) override
    { ++RecoveryRequests; RequestedMap = Map; RequestedOptions = Options; }
};
