// Copyright Fallen Signal Studios. All Rights Reserved.
//
// Covers the missing edge behind the reproduced Aurelion AI startup stall
// (Docs/AIStartupRaceReproduction-2026-09-11.md).
//
// An NPC that perceived the player BEFORE the player's factions were published resolved
// Neutral, rejected the target, and then never reconsidered: UE 5.7 suppresses the
// same-state Sight notification, and a faction MEMBERSHIP change raised no faction event
// at all. Every input that could correct the decision had either already fired or was
// suppressed, so the NPC stayed in its fallback tree with zero goals while actively
// seeing a hostile player.
//
// These tests pin the repair's contract rather than its implementation:
//   - a membership change reaches an NPC that already perceives the changed actor
//   - it does not reach NPCs that do not perceive it (one publication must not touch
//     every NPC in the level)
//   - it is refused without authority (goals are server-authoritative)
//   - the default reevaluation forwards to a generator's authored zero-parameter
//     RefreshPerceivedActors pass, and refuses any other signature
//   - repeated reevaluation cannot duplicate a goal for a target already registered
#include "Tests/SovRuntimeObjectTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalGenerator.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "Tests/SovFactionMembershipReevaluationTestFixtures.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/**
	 * A game world carrying a real ANarrativeGameState, because the membership signal is
	 * brokered through it and the activity component subscribes during BeginPlay.
	 */
	struct FMembershipWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovFactionTestGameState* GameState = nullptr;

		FMembershipWorld()
		{
			const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false)
				.SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &WorldInitialization);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }

			// Set before any controller begins play: the subscription is made from
			// UNPCActivityComponent::BeginPlay and a missing game state simply skips it.
			GameState = World->SpawnActor<ASovFactionTestGameState>();
			World->SetGameState(GameState);
			World->InitializeActorsForPlay(FURL());
		}

		~FMembershipWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}

		ASovBotTestCharacter* Character(const FVector Position, const int32 Team)
		{
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Spawned = World->SpawnActor<ASovBotTestCharacter>(
				ASovBotTestCharacter::StaticClass(), Position, FRotator::ZeroRotator, Parameters);
			if (Spawned) { Spawned->InitializeTestCombat(Team); }
			return Spawned;
		}

		/** A possessed NPC controller whose activity component has begun play and subscribed. */
		ANarrativeNPCController* Controller(ASovBotTestCharacter* Pawn)
		{
			auto* Spawned = World->SpawnActor<ANarrativeNPCController>();
			if (!Spawned) { return nullptr; }
			Spawned->Possess(Pawn);
			Spawned->bShareThreatsWithFaction = false;
			Spawned->DispatchBeginPlay();
			return Spawned;
		}

		/** Give the controller sight, and optionally make it currently perceive Target. */
		UAIPerceptionComponent* Perceive(ANarrativeNPCController* AI, AActor* Target, const bool bSeen)
		{
			auto* Perception = NewObject<UAIPerceptionComponent>(AI);
			AI->AddInstanceComponent(Perception);
			auto* Sight = NewObject<UAISenseConfig_Sight>(Perception);
			Sight->DetectionByAffiliation.bDetectEnemies = true;
			Sight->DetectionByAffiliation.bDetectNeutrals = true;
			Sight->DetectionByAffiliation.bDetectFriendlies = true;
			Perception->ConfigureSense(*Sight);
			AI->SetPerceptionComponent(*Perception);
			Perception->RegisterComponent();
			if (bSeen && Target)
			{
				FAIStimulus Stimulus(*GetDefault<UAISense_Sight>(), 1.f,
					Target->GetActorLocation(), AI->GetPawn()->GetActorLocation());
				Perception->RegisterStimulus(Target, Stimulus);
				Perception->ProcessStimuli();
			}
			return Perception;
		}

		/**
		 * A narrative player character with a player state bound both ways, so SetFactions
		 * drives the real publication path rather than a test-only shortcut.
		 */
		ASovFactionTestPlayerCharacter* PlayerPawnWithState(const FVector Position, ASovFactionTestPlayerState*& OutState)
		{
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ASovFactionTestPlayerCharacter>(
				ASovFactionTestPlayerCharacter::StaticClass(), Position, FRotator::ZeroRotator, Parameters);
			OutState = World->SpawnActor<ASovFactionTestPlayerState>();
			if (Pawn && OutState) { Pawn->SetPlayerState(OutState); }
			return Pawn;
		}
	};

	FGameplayTagContainer HeroFactions()
	{
		FGameplayTagContainer Factions;
		Factions.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"), false));
		return Factions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMembershipChangeReachesPerceivingNPCTest,
	"ProjectVelkorran.Campaign.AI.FactionMembershipChangeReachesPerceivingNPC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovMembershipChangeReachesPerceivingNPCTest::RunTest(const FString& Parameters)
{
	FMembershipWorld Fixture;
	if (!TestNotNull(TEXT("Game world"), Fixture.World)
		|| !TestNotNull(TEXT("Narrative game state"), Fixture.GameState)) { return false; }

	ASovBotTestCharacter* Hostile = Fixture.Character(FVector::ZeroVector, 1);
	ASovFactionTestPlayerState* PlayerState = nullptr;
	ASovFactionTestPlayerCharacter* Player = Fixture.PlayerPawnWithState(FVector(400, 0, 0), PlayerState);
	ANarrativeNPCController* AI = Fixture.Controller(Hostile);
	if (!TestNotNull(TEXT("Hostile"), Hostile) || !TestNotNull(TEXT("Player pawn"), Player)
		|| !TestNotNull(TEXT("Player state"), PlayerState)
		|| !TestNotNull(TEXT("NPC controller"), AI)) { return false; }
	if (!TestEqual(TEXT("Player state resolves the pawn faction publication reads"),
		PlayerState->GetPawn(), static_cast<APawn*>(Player))) { return false; }

	// Order is the whole defect: the NPC perceives the player while the player is still
	// factionless, which resolves Neutral and produces no attack goal.
	Fixture.Perceive(AI, Player, true);

	auto* Generator = Cast<USovReevaluationCountingGenerator>(
		AI->GetActivityComponent()->AddGoalGenerator(USovReevaluationCountingGenerator::StaticClass(), false));
	if (!TestNotNull(TEXT("Registered counting generator"), Generator)) { return false; }

	TestEqual(TEXT("No reconsideration has been requested before the factions are published"),
		Generator->ReevaluationCount, 0);

	// The real publication path, not a shortcut: SetFactions -> OnRep_Faction -> game state.
	PlayerState->SetFactions(HeroFactions());

	TestEqual(TEXT("Publishing the player's factions reaches the NPC that already perceives them"),
		Generator->ReevaluationCount, 1);

	// Repeats are permitted and must stay well defined - one request per publication, and
	// never a compounding cascade.
	PlayerState->SetFactions(HeroFactions());
	TestEqual(TEXT("A second publication is delivered once more, not amplified"),
		Generator->ReevaluationCount, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMembershipChangeSkipsUnrelatedNPCTest,
	"ProjectVelkorran.Campaign.AI.FactionMembershipChangeSkipsUnperceivingNPC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovMembershipChangeSkipsUnrelatedNPCTest::RunTest(const FString& Parameters)
{
	FMembershipWorld Fixture;
	if (!TestNotNull(TEXT("Game world"), Fixture.World)
		|| !TestNotNull(TEXT("Narrative game state"), Fixture.GameState)) { return false; }

	ASovBotTestCharacter* Hostile = Fixture.Character(FVector::ZeroVector, 1);
	ASovBotTestCharacter* Player = Fixture.Character(FVector(400, 0, 0), 0);
	ANarrativeNPCController* AI = Fixture.Controller(Hostile);
	if (!TestNotNull(TEXT("Hostile"), Hostile) || !TestNotNull(TEXT("Player stand-in"), Player)
		|| !TestNotNull(TEXT("NPC controller"), AI)) { return false; }

	// Sight configured, but this NPC is not currently perceiving anyone.
	Fixture.Perceive(AI, Player, false);

	auto* Generator = Cast<USovReevaluationCountingGenerator>(
		AI->GetActivityComponent()->AddGoalGenerator(USovReevaluationCountingGenerator::StaticClass(), false));
	if (!TestNotNull(TEXT("Registered counting generator"), Generator)) { return false; }

	Fixture.GameState->NotifyFactionMembershipChanged(Player, HeroFactions());

	// An NPC that cannot see the actor holds no stale decision about it: whenever it does
	// perceive it, that event evaluates against the factions in force at the time.
	TestEqual(TEXT("A membership change does not reach an NPC that is not perceiving that actor"),
		Generator->ReevaluationCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReevaluationRequiresAuthorityTest,
	"ProjectVelkorran.Campaign.AI.ReevaluationRequiresAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovReevaluationRequiresAuthorityTest::RunTest(const FString& Parameters)
{
	FMembershipWorld Fixture;
	if (!TestNotNull(TEXT("Game world"), Fixture.World)
		|| !TestNotNull(TEXT("Narrative game state"), Fixture.GameState)) { return false; }

	ASovBotTestCharacter* Hostile = Fixture.Character(FVector::ZeroVector, 1);
	ASovBotTestCharacter* Player = Fixture.Character(FVector(400, 0, 0), 0);
	ANarrativeNPCController* AI = Fixture.Controller(Hostile);
	if (!TestNotNull(TEXT("Hostile"), Hostile) || !TestNotNull(TEXT("Player stand-in"), Player)
		|| !TestNotNull(TEXT("NPC controller"), AI)) { return false; }

	Fixture.Perceive(AI, Player, true);
	auto* Generator = Cast<USovReevaluationCountingGenerator>(
		AI->GetActivityComponent()->AddGoalGenerator(USovReevaluationCountingGenerator::StaticClass(), false));
	if (!TestNotNull(TEXT("Registered counting generator"), Generator)) { return false; }

	// Goals are produced on the authority and replicated. A simulated proxy must not
	// generate its own, even though it perceives the actor and hears the signal.
	AI->SetRole(ROLE_SimulatedProxy);
	Fixture.GameState->NotifyFactionMembershipChanged(Player, HeroFactions());
	TestEqual(TEXT("Reevaluation is refused without authority"), Generator->ReevaluationCount, 0);

	AI->SetRole(ROLE_Authority);
	Fixture.GameState->NotifyFactionMembershipChanged(Player, HeroFactions());
	TestEqual(TEXT("The same signal is honoured once authority is restored"),
		Generator->ReevaluationCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAuthoredRefreshEntryPointTest,
	"ProjectVelkorran.Campaign.AI.ReevaluationUsesAuthoredRefreshEntryPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAuthoredRefreshEntryPointTest::RunTest(const FString& Parameters)
{
	FMembershipWorld Fixture;
	if (!TestNotNull(TEXT("Game world"), Fixture.World)) { return false; }

	auto* Authored = NewObject<USovAuthoredRefreshGenerator>(Fixture.World);
	auto* Mismatched = NewObject<USovMismatchedRefreshGenerator>(Fixture.World);
	if (!TestNotNull(TEXT("Authored generator"), Authored)
		|| !TestNotNull(TEXT("Mismatched generator"), Mismatched)) { return false; }

	// The shipped GoalGenerator_Attack keeps its hostility predicate inside this pass. The
	// default hook forwards to it so the predicate is never restated in native code.
	Authored->ReevaluatePerceivedActors();
	TestEqual(TEXT("The default hook invokes an authored zero-parameter refresh pass"),
		Authored->RefreshCount, 1);

	// Same name, different contract. Calling it with an empty frame would read garbage
	// parameters, so it must be left alone rather than invoked.
	Mismatched->ReevaluatePerceivedActors();
	TestEqual(TEXT("A refresh pass with a different signature is not invoked"),
		Mismatched->RefreshCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReevaluationCannotDuplicateGoalsTest,
	"ProjectVelkorran.Campaign.AI.ReevaluationCannotDuplicateGoalsForSameTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovReevaluationCannotDuplicateGoalsTest::RunTest(const FString& Parameters)
{
	FMembershipWorld Fixture;
	if (!TestNotNull(TEXT("Game world"), Fixture.World)) { return false; }

	ASovBotTestCharacter* Hostile = Fixture.Character(FVector::ZeroVector, 1);
	ASovBotTestCharacter* Player = Fixture.Character(FVector(400, 0, 0), 0);
	ANarrativeNPCController* AI = Fixture.Controller(Hostile);
	if (!TestNotNull(TEXT("Hostile"), Hostile) || !TestNotNull(TEXT("Player stand-in"), Player)
		|| !TestNotNull(TEXT("NPC controller"), AI)) { return false; }

	UClass* GoalClass = LoadClass<UNPCGoalItem>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
	if (!TestNotNull(TEXT("Shipped attack goal class"), GoalClass)) { return false; }

	UNPCActivityComponent* Activities = AI->GetActivityComponent();
	const FObjectPropertyBase* TargetField = FindFProperty<FObjectPropertyBase>(GoalClass, TEXT("TargetToAttack"));
	if (!TestNotNull(TEXT("Activity component"), Activities)
		|| !TestNotNull(TEXT("Stock goal's target field"), TargetField)) { return false; }

	const auto MakeGoalFor = [&](AActor* Target) -> UNPCGoalItem*
	{
		UNPCGoalItem* Goal = NewObject<UNPCGoalItem>(Activities, GoalClass);
		TargetField->SetObjectPropertyValue_InContainer(Goal, Target);
		return Goal;
	};

	TestNotNull(TEXT("The first attack goal for a target is admitted"),
		Activities->AddGoal(MakeGoalFor(Player), false));

	// A reevaluation pass builds a fresh candidate and offers it again. Admission keys on the
	// target, so the second candidate must be rejected - this is what keeps the repair from
	// giving the runs that already recover a second goal.
	TestNull(TEXT("A second distinct goal for the same target is rejected"),
		Activities->AddGoal(MakeGoalFor(Player), false));

	const FNPCGoalContainer Goals = Activities->GetGoals(GoalClass);
	TestEqual(TEXT("Exactly one attack goal exists for the target"), Goals.Goals.Num(), 1);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
