// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "BrainComponent.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "Campaign/SovEncounterTypes.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "SovEncounterObjectiveRuntimeTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovEncounterObjectiveTestObserver : public UObject
{
    GENERATED_BODY()
public:
    FString RestoreError;
    UFUNCTION() void RestoreFailed(const FString& Error) { RestoreError = Error; }
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

/** Native subobject survives actual encounter respawn; no test-injected wave receipt. */
UCLASS(Transient, NotBlueprintable)
class ASovWaveReleaseTestNPC : public ASovCampaignMassRoundTripNPC
{
    GENERATED_BODY()
public:
    ASovWaveReleaseTestNPC(const FObjectInitializer& Initializer);
    void SetSnapshotReadyForTest(bool bReady) { bEncounterSnapshotReady = bReady; }
};

/** Observable brain lifecycle at the real controller's public brain slot; director ownership stays native. */
UCLASS(Transient, NotBlueprintable)
class USovLoadedEncounterTestBrain : public UBrainComponent
{
    GENERATED_BODY()
public:
    int32 PauseCount = 0;
    bool bPausedForTest = false;
    virtual bool IsRunning() const override { return true; }
    virtual bool IsPaused() const override { return bPausedForTest; }
    virtual void PauseLogic(const FString& Reason) override { bPausedForTest = true; ++PauseCount; }
    virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override
    { const auto Result = Super::ResumeLogic(Reason); bPausedForTest = false; return Result; }
};

/** Shared role class with an independent default tag; identities are authored on instances. */
UCLASS(Transient, NotBlueprintable)
class ASovEncounterStoryIdentityNPC : public ASovCampaignMassRoundTripNPC
{
    GENERATED_BODY()
public:
    ASovEncounterStoryIdentityNPC(const FObjectInitializer& Initializer);
    virtual void PostInitializeComponents() override;
    bool bHadStoryIdentityAtInitialization = false;
};
