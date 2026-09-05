// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "Corruption/SovCorruptionProfile.h"
#include "Components/SovCommandLinkComponent.h"
#include "GAS/SovCombatTypes.h"
#include "Campaign/SovEncounterDirector.h"
#include "SovCorruptionRuntimeTestFixtures.generated.h"

/** Uses the actual player component and Narrative ASC while excluding content initialization. */
UCLASS(Transient, NotBlueprintable)
class ASovCorruptionRuntimeTestPawn : public ASovCampaignRuntimeTestPawn
{
	GENERATED_BODY()
public:
	ASovCorruptionRuntimeTestPawn(const FObjectInitializer& ObjectInitializer);
	void InitializeCombat();
	void SetReadyForRestoreTest(bool bReady) { bCharacterReady = bReady; }
	UFUNCTION() void SaveOnBandChanged(ESovCorruptionBand Previous, ESovCorruptionBand Current);
	int32 SaveCallbacks = 0;
	ETeamAttitude::Type Attitude = ETeamAttitude::Hostile;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override { return Attitude; }
	UFUNCTION() void ObserveDamage(const FSovDamageResult& Result) { LastDamage = Result; }
	FSovDamageResult LastDamage;
};

UCLASS(Transient, NotBlueprintable)
class USovCorruptionRuntimeTestLink : public USovCommandLinkComponent
{
	GENERATED_BODY()
public:
	USovCorruptionRuntimeTestLink() { LinkId = TEXT("CorruptionLink"); bStartsActive = false; bRevealWeakPointsOnSever = false; }
};

UCLASS(Transient, NotBlueprintable)
class ASovCorruptionRuntimeTestDirector : public ASovEncounterDirector
{
	GENERATED_BODY()
public:
	void Arm() { EncounterId = TEXT("CorruptionClock"); bHasEntryCheckpoint = true; State = ESovEncounterState::Active; AttemptId = FGuid::NewGuid(); }
};
