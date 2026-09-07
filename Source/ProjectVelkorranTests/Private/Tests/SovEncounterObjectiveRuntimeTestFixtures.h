// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Campaign/SovEncounterTypes.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "SovEncounterObjectiveRuntimeTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovEncounterObjectiveTestObserver : public UObject
{
    GENERATED_BODY()
public:
    TFunction<void()> OnVictory;
    TFunction<void()> OnCommit;
    UFUNCTION() void EncounterChanged(ESovEncounterState Previous, ESovEncounterState Current)
    { if (Current == ESovEncounterState::Succeeded && OnVictory) { OnVictory(); } }
    UFUNCTION() void BeatCommitted(const FSovCampaignJournalEntry& Entry)
    { if (OnCommit) { OnCommit(); } }
};

/** Asset/visual readiness fixture with the real Selene identity for native Crucible severs. */
UCLASS(Transient, NotBlueprintable)
class ASovCrucibleTestSelenePawn : public ASovHandoffRuntimeTestPawn
{
    GENERATED_BODY()
public:
    virtual FGameplayTag GetProtagonistIdentityTag() const override;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};
