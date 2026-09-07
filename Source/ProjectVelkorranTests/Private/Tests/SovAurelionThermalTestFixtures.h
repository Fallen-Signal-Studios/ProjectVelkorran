// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Campaign/SovEncounterDirector.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "SovAurelionThermalTestFixtures.generated.h"

/** Supplies an external already-active encounter. No fracture outcome or damage receipt is seeded. */
UCLASS(Transient, NotBlueprintable)
class ASovAurelionThermalTestDirector : public ASovEncounterDirector
{
    GENERATED_BODY()
public:
    virtual ESovEncounterProofType GetCampaignProofType() const override { return ESovEncounterProofType::AurelionThermalFracture; }
    void StartTestAttempt()
    {
        const auto Previous = State;
        State = ESovEncounterState::Active; AttemptId = FGuid::NewGuid(); bHasEntryCheckpoint = true;
        OnEncounterStateChanged.Broadcast(Previous, State);
    }
};
UCLASS(Transient, NotBlueprintable)
class ASovAurelionThermalTestPlayer : public ASovHandoffRuntimeTestPawn
{
    GENERATED_BODY()
public:
    ASovAurelionThermalTestPlayer(const FObjectInitializer& Initializer) : Super(Initializer) {}
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override
    { return Other.IsA<ASovProtagonistCompanionCharacter>() || &Other == this ? ETeamAttitude::Friendly : ETeamAttitude::Hostile; }
};
/** Replaces only content/visual initialization for the real protagonist proxy class and ASC. */
UCLASS(Transient, NotBlueprintable)
class ASovAurelionThermalTestCompanion : public ASovProtagonistCompanionCharacter
{
    GENERATED_BODY()
public:
    ASovAurelionThermalTestCompanion(const FObjectInitializer& Initializer) : Super(Initializer) {}
    void InitializeTestCombat();
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override
    { return Other.IsA<ASovPlayerCharacterBase>() || &Other == this ? ETeamAttitude::Friendly : ETeamAttitude::Hostile; }
protected:
    virtual void SpawnDefaultController() override {}
};
