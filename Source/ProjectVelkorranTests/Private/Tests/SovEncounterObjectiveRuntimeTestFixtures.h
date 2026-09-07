// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
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
