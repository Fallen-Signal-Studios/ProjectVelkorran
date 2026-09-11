// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "SovAurelionCrossfireTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class ASovAurelionCrossfireTestDrone : public ASovAurelionSecurityDrone
{
    GENERATED_BODY()
public:
    ASovAurelionCrossfireTestDrone(const FObjectInitializer& Initializer) : Super(Initializer) {}
    TWeakObjectPtr<AActor> Hostile;
    void InitializeTestRole();
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override
    { return Hostile.Get() == &Other ? ETeamAttitude::Hostile : ETeamAttitude::Friendly; }
};
