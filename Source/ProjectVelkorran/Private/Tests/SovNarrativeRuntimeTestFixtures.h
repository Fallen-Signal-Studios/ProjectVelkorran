// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "SovNarrativeRuntimeTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class ASovNarrativeRuntimeTestPawn : public ASovCampaignRuntimeTestPawn
{
    GENERATED_BODY()
public:
    ASovNarrativeRuntimeTestPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
    bool bTestAlive = true;
    virtual bool IsAlive() const override { return bTestAlive; }
    void SetTestReady(bool bReady) { bCharacterReady = bReady; }
};

UCLASS(Transient, NotBlueprintable)
class ASovNarrativeRuntimeTestController : public ASovCampaignRuntimeTestController
{
    GENERATED_BODY()
public:
    virtual void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const override
    {
        Location = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
        Rotation = GetControlRotation();
    }
};
