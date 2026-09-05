// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Campaign/SovEncounterDirector.h"
#include "SovEncounterRuntimeTestFixtures.generated.h"

/** Seeds state only for ledger/save-policy tests; entry/retry integration still needs authored NPC definitions. */
UCLASS(Transient, NotBlueprintable)
class ASovEncounterRuntimeTestDirector : public ASovEncounterDirector
{
	GENERATED_BODY()
public:
	void SeedState(ESovEncounterState Value) { State = Value; AttemptId = FGuid::NewGuid(); }
	void SeedRecoveryCheckpoint() { bHasEntryCheckpoint = true; }
	int32 SeverEventCount = 0;
	UFUNCTION() void ObserveSever(const FSovCommandLinkSeverResult& Result) { ++SeverEventCount; }
};
