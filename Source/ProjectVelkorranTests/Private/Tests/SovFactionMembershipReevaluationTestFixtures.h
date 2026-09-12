// Copyright Fallen Signal Studios. All Rights Reserved.
// Generators used to pin the reconsideration contract added for the Aurelion AI startup
// stall. See SovFactionMembershipReevaluationTests.cpp for what each one proves.
#pragma once
#include "AI/Activities/NPCGoalGenerator.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "SovFactionMembershipReevaluationTestFixtures.generated.h"

// ANarrativeGameState is a stable actor, and the save subsystem asserts on any spawned stable
// actor that does not answer GetActorGUID. These fixtures are spawned into a bare test world
// rather than loaded from a level, so it supplies its own identity as the bot fixtures do.

/**
 * A player pawn of the type ANarrativePlayerState::OnRep_Faction actually looks for. Faction
 * publication only reaches the rest of the game when the possessed pawn is a narrative player
 * character, so the real path cannot be driven with an NPC stand-in.
 */
UCLASS(Transient, NotBlueprintable, NotPlaceable)
class ASovFactionTestPlayerCharacter : public ANarrativePlayerCharacter
{
	GENERATED_BODY()
};

/** The broker the reconsideration signal travels through. */
UCLASS(Transient, NotBlueprintable, NotPlaceable)
class ASovFactionTestGameState : public ANarrativeGameState
{
	GENERATED_BODY()
public:
	virtual FGuid GetActorGUID_Implementation() const override { return TestActorGuid; }
	FGuid TestActorGuid = FGuid::NewGuid();
};

/** Owns the factions whose publication is the event under test. */
UCLASS(Transient, NotBlueprintable, NotPlaceable)
class ASovFactionTestPlayerState : public ANarrativePlayerState
{
	GENERATED_BODY()
};

/** Records reconsideration requests. Overrides the hook, so the name bridge is not used. */
UCLASS(Transient, NotBlueprintable)
class USovReevaluationCountingGenerator : public UNPCGoalGenerator
{
	GENERATED_BODY()
public:
	int32 ReevaluationCount = 0;
	virtual void ReevaluatePerceivedActors_Implementation() override { ++ReevaluationCount; }
};

/**
 * Stands in for the shipped GoalGenerator_Attack, which exposes its reevaluation pass as a
 * public zero-parameter Blueprint function called RefreshPerceivedActors. Deliberately does
 * NOT override the hook, so the default implementation's bridge is what is under test.
 */
UCLASS(Transient, NotBlueprintable)
class USovAuthoredRefreshGenerator : public UNPCGoalGenerator
{
	GENERATED_BODY()
public:
	int32 RefreshCount = 0;

	UFUNCTION(BlueprintCallable, Category = "Test")
	void RefreshPerceivedActors() { ++RefreshCount; }
};

/** Same name, wrong shape. The bridge must refuse to invoke it with an empty frame. */
UCLASS(Transient, NotBlueprintable)
class USovMismatchedRefreshGenerator : public UNPCGoalGenerator
{
	GENERATED_BODY()
public:
	int32 RefreshCount = 0;

	UFUNCTION(BlueprintCallable, Category = "Test")
	void RefreshPerceivedActors(int32 Unused) { ++RefreshCount; }
};
