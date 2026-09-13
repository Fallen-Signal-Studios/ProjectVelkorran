// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "AI/SovAurelionEnemyRoles.h"
#include "BrainComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "SovAurelionEnemyRoleTestFixtures.generated.h"

/** Runs the real component bootstrap/timer in the isolated role world. */
UCLASS(Transient, NotBlueprintable)
class USovAurelionBootstrapTestLink : public USovAurelionFreshCommandLink
{
    GENERATED_BODY()
public:
    void StartBootstrapForTest() { if (!HasBegunPlay()) { BeginPlay(); } }
};

UCLASS(Transient, NotBlueprintable)
class ASovAurelionTestWallRunner : public ASovAurelionWallRunner
{
    GENERATED_BODY()
public:
    ASovAurelionTestWallRunner(const FObjectInitializer& Initializer) : Super(Initializer) {}
    TFunction<void()> OnTestMovementModeChanged;
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode = 0) override
    {
        Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
        if (OnTestMovementModeChanged) { OnTestMovementModeChanged(); }
    }
    void InitializeTestRole();
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override { return ETeamAttitude::Friendly; }
};

UCLASS(Transient, NotBlueprintable)
class ASovAurelionTestWeaver : public ASovAurelionWeaver
{
    GENERATED_BODY()
public:
    ASovAurelionTestWeaver(const FObjectInitializer& Initializer) : Super(Initializer) {}
    void InitializeTestRole();
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override { return ETeamAttitude::Friendly; }
};

/** Admission fixture only: tests phase/attempt fences without fabricating a gameplay victory. */
UCLASS(Transient, NotBlueprintable)
class ASovAurelionTraversalTestDirector : public ASovEncounterDirector
{
    GENERATED_BODY()
public:
    void SetAdmissionState(ESovEncounterState NewState, const FGuid& NewAttempt)
    { State = NewState; AttemptId = NewAttempt; }
};

/** Observable pause on the real controller; traversal movement and director suspension stay native. */
UCLASS(Transient, NotBlueprintable)
class USovAurelionTraversalTestBrain : public UBrainComponent
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
