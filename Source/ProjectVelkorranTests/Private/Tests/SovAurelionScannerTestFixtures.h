// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Characters/SovDroneNPCBase.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "SovAurelionScannerTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class ASovAurelionScannerTestPlayer : public ASovHandoffRuntimeTestPawn
{
    GENERATED_BODY()
public:
    ASovAurelionScannerTestPlayer(const FObjectInitializer& Initializer) : Super(Initializer) {}
    virtual FGameplayTag GetProtagonistIdentityTag() const override;
};

UCLASS(Transient, NotBlueprintable)
class ASovAurelionScannerTestDrone : public ASovDroneNPCBase
{
    GENERATED_BODY()
public:
    ASovAurelionScannerTestDrone(const FObjectInitializer& Initializer) : Super(Initializer) {}
    void InitializeTestCombat();
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override
    { return Other.IsA<ASovPlayerCharacterBase>() ? ETeamAttitude::Hostile : ETeamAttitude::Friendly; }
protected:
    virtual void SpawnDefaultController() override {}
};
