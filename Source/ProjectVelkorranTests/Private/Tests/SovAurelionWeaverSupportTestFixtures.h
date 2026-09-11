// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovAurelionEnemyRoleTestFixtures.h"
#include "SovAurelionWeaverSupportTestFixtures.generated.h"

/** Existing native ready-NPC fixture with one explicit hostile; all linked members remain friendly. */
UCLASS(Transient, NotBlueprintable)
class ASovAurelionSupportTestWeaver : public ASovAurelionTestWeaver
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<AActor> Hostile;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override
    { return &Other == Hostile.Get() ? ETeamAttitude::Hostile : ETeamAttitude::Friendly; }
};
