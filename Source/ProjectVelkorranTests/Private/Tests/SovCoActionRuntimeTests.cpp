// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCoActionRuntimeTestFixtures.h"
#include "Tests/SovEncounterRuntimeTestFixtures.h"
#include "Companions/SovCoActionAnchor.h"
#include "Companions/SovCoActionActivity.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovCompanionCommandActivity.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
/** Models path-completion delivery, never bypassing native mission permission/receipt validation. */
struct FSovCoActionTestAccess
{
	static USovCoActionGoal* Goal(USovCompanionComponent& Component) { return Component.ActiveGoal; }
	static void Tick(USovCompanionComponent& Component) { Component.TickComponent(0.016f, LEVELTICK_All, nullptr); }
	static void Expire(USovCompanionComponent& Component) { Component.RequestedAt = -100.f; }
	static bool Hidden(USovCompanionComponent& Component, FVector Destination) { return Component.IsFallbackHiddenFromAllPlayers(Destination); }
	static ESovCampaignResult Forge(USovCampaignStateComponent& State, ASovCoActionAnchor* Anchor) { return State.CompleteCoAction(Anchor); }
	static void CorruptProof(USovCampaignStateComponent& State) { State.Journal.Last().CoActionRequestId.Invalidate(); }
	static void Death(USovCompanionComponent& Component)
	{ Component.HandleDeath(Component.GetOwner(), Component.BoundASC, true); }
	static void SeedLeader(USovCompanionComponent& Component, ASovPlayerCharacterBase* Leader, UNarrativeAbilitySystemComponent* ASC)
	{ Component.Leader = Leader; Component.BoundASC = ASC; }
	static USovCompanionCommandGoal* CommandGoal(USovCompanionComponent& Component) { return Component.CommandGoal; }
};

namespace
{
	struct FCoActionWorld
	{
		UWorld* World = nullptr;
		ASovCampaignRuntimeTestController* PC = nullptr;
		ASovCoActionTestPlayer* Player = nullptr;
		ASovCoActionTestNPC* NPC = nullptr;
		ASovCoActionTestNPCController* AI = nullptr;
		USovCompanionComponent* Companion = nullptr;
		ASovCoActionAnchor* Anchor = nullptr;
		USovCampaignDefinition* Mission = nullptr;
		FCoActionWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			PC = World->SpawnActor<ASovCoActionTestPlayerController>();
			Player = World->SpawnActor<ASovCoActionTestPlayer>();
			NPC = World->SpawnActor<ASovCoActionTestNPC>();
			AI = World->SpawnActor<ASovCoActionTestNPCController>();
			Anchor = World->SpawnActor<ASovCoActionAnchor>();
			if (!PC || !Player || !NPC || !AI || !Anchor) { return; }
			Player->TestHero = FSovGameplayTags::Get().Character_Player_Selene;
			Player->InitializeForCoAction(); PC->Possess(Player);
			NPC->InitializeTestCombat(0); AI->Possess(NPC);
			CastChecked<USovCoActionTestActivities>(AI->GetActivityComponent())->InitializeForCoAction();
			Companion = NewObject<USovCompanionComponent>(NPC); NPC->AddInstanceComponent(Companion); Companion->RegisterComponent();
			Companion->CompanionId = TEXT("Lyric");
			Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
			auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
			Mission->MissionId = TEXT("CoActionTest"); Mission->Protagonist = Player->TestHero;
			Mission->PawnClass = ASovCoActionTestPlayer::StaticClass(); Mission->PlayerDefinition = Definition;
			FSovCampaignBeatDefinition Escape; Escape.BeatId = TEXT("Escape"); Mission->Beats.Add(Escape);
			FSovCampaignBeatDefinition Arrival; Arrival.BeatId = TEXT("Arrival"); Arrival.PrerequisiteBeats.Add(Escape.BeatId);
			Arrival.bRequiresCoActionProof = true; Arrival.RequiredCompanionId = TEXT("Lyric"); Arrival.RequiredCoActionAnchorId = TEXT("ExtractionMark");
			Mission->Beats.Add(Arrival);
			FSovCampaignBeatDefinition Extraction; Extraction.BeatId = TEXT("Extraction"); Extraction.CinematicId = TEXT("ExtractionSequence");
			Extraction.PrerequisiteBeats.Add(Arrival.BeatId); Mission->Beats.Add(Extraction);
			Anchor->AnchorId = Arrival.RequiredCoActionAnchorId; Anchor->RequiredCompanionId = Arrival.RequiredCompanionId;
			Anchor->MissionId = Mission->MissionId; Anchor->CompletionBeat = Arrival.BeatId; Anchor->HoldAtMarkSeconds = 0.f;
			Anchor->SetActorLocation(NPC->GetNavAgentLocation());
			PC->State->BeginMission(Mission);
		}
		~FCoActionWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		bool Ready() const { return World && PC && Player && NPC && AI && Anchor && Companion && Mission; }
		void Arrive()
		{
			Companion->NotifyPathResult(FSovCoActionTestAccess::Goal(*Companion), true);
			FSovCoActionTestAccess::Tick(*Companion);
		}
		AActor* Wall(FVector Location)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(20.f, 500.f, 500.f)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(Location); return Actor;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCoActionProofTest, "ProjectVelkorran.Campaign.Companion.NativeArrivalProof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCoActionProofTest::RunTest(const FString& Parameters)
{
	FCoActionWorld F; if (!TestTrue(TEXT("World fixture"), F.Ready())) { return false; } FString Reason;
	auto* Opening = NewObject<USovOneDegreeMissionDefinition>(F.PC); F.PC->KeepAlive.Add(Opening);
	Opening->PlayerDefinition = F.Mission->PlayerDefinition;
	TestTrue(TEXT("Native M02 schema remains valid"), Opening->ValidateDefinition(Reason));
	const auto* NativeArrival = Opening->FindBeat(TEXT("LyricAtExtraction"));
	const auto* NativeExtraction = Opening->FindBeat(TEXT("LyricExtraction"));
	TestTrue(TEXT("M02 cinematic requires the exact native Lyric arrival"), NativeArrival && NativeArrival->bRequiresCoActionProof
		&& NativeArrival->RequiredCompanionId == TEXT("Lyric") && NativeArrival->RequiredCoActionAnchorId == TEXT("M02_LyricExtraction")
		&& NativeExtraction && NativeExtraction->PrerequisiteBeats.Contains(TEXT("LyricAtExtraction")));
	TestFalse(TEXT("Mission prerequisites gate request"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	F.PC->State->CompleteBeat(TEXT("Escape"));
	TestEqual(TEXT("Generic completion cannot invent arrival"), F.PC->State->CompleteBeat(TEXT("Arrival")), ESovCampaignResult::Invalid);
	TestEqual(TEXT("An authored anchor without a live receipt is insufficient"), FSovCoActionTestAccess::Forge(*F.PC->State, F.Anchor), ESovCampaignResult::Invalid);
	TestEqual(TEXT("Extraction remains gated"), F.PC->State->CompleteBeat(TEXT("Extraction")), ESovCampaignResult::PrerequisiteMissing);
	TestTrue(TEXT("Request acquires real Narrative goal/activity"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	TestTrue(TEXT("Existing activity slot owns the exact request"), F.AI->GetActivityComponent()->GetCurrentActivityGoal() == FSovCoActionTestAccess::Goal(*F.Companion));
	F.Arrive();
	TestEqual(TEXT("Valid native arrival completes action"), F.Companion->GetCommandState(), ESovCompanionCommandState::Succeeded);
	TestTrue(TEXT("Mission stores arrival fact"), F.PC->State->IsBeatComplete(F.Mission->MissionId, TEXT("Arrival")));
	const auto& Entry = F.PC->State->GetJournal().Last();
	TestTrue(TEXT("Durable proof identifies exact companion and anchor"), Entry.CoActionRequestId.IsValid()
		&& Entry.CoActionCompanionId == TEXT("Lyric") && Entry.CoActionAnchorId == TEXT("ExtractionMark"));
	TestFalse(TEXT("Completed action cannot issue another reward"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	F.PC->State->Load_Implementation();
	TestTrue(TEXT("Arrival fact survives save-state validation without transient goal"), F.PC->State->IsStateValid());
	FSovCoActionTestAccess::CorruptProof(*F.PC->State); F.PC->State->Load_Implementation();
	TestFalse(TEXT("Missing saved receipt invalidates claimed companion fact"), F.PC->State->IsStateValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCoActionCancellationTest, "ProjectVelkorran.Campaign.Companion.CancelLoadAndStaleCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCoActionCancellationTest::RunTest(const FString& Parameters)
{
	FCoActionWorld F; if (!TestTrue(TEXT("World fixture"), F.Ready())) { return false; } FString Reason;
	F.PC->State->CompleteBeat(TEXT("Escape"));
	TestTrue(TEXT("Request accepted"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	USovCoActionGoal* StaleGoal = FSovCoActionTestAccess::Goal(*F.Companion);
	auto* ASC = F.NPC->GetNarrativeAbilitySystemComponent(); const FGameplayTag Busy = FNarrativeGameplayTags::Get().State_Busy;
	TestEqual(TEXT("One owned Busy contribution"), ASC->GetTagCount(Busy), 1);
	ASC->AddLooseGameplayTag(Busy);
	F.Companion->CancelCoAction();
	TestEqual(TEXT("Cancel preserves another system's Busy contribution"), ASC->GetTagCount(Busy), 1);
	F.Companion->NotifyPathResult(StaleGoal, true); FSovCoActionTestAccess::Tick(*F.Companion);
	TestFalse(TEXT("Late path completion cannot commit canceled request"), F.PC->State->IsBeatComplete(F.Mission->MissionId, TEXT("Arrival")));
	ASC->RemoveLooseGameplayTag(Busy);
	TestTrue(TEXT("Retry is allowed after cancellation"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	F.Companion->Load_Implementation();
	TestEqual(TEXT("Load clears in-progress action"), F.Companion->GetCommandState(), ESovCompanionCommandState::Idle);
	TestEqual(TEXT("Load releases own Busy"), ASC->GetTagCount(Busy), 0);
	TestFalse(TEXT("Load does not save or resurrect transient goal"), F.AI->GetActivityComponent()->HasGoal(USovCoActionGoal::StaticClass()));
	TestTrue(TEXT("Prior mission facts survive command cleanup"), F.PC->State->IsBeatComplete(F.Mission->MissionId, TEXT("Escape")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCoActionFailureTest, "ProjectVelkorran.Campaign.Companion.TimeoutInterruptionAndDefeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCoActionFailureTest::RunTest(const FString& Parameters)
{
	FCoActionWorld F; if (!TestTrue(TEXT("World fixture"), F.Ready())) { return false; } FString Reason;
	F.PC->State->CompleteBeat(TEXT("Escape"));
	TestTrue(TEXT("Request accepted"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	F.Companion->NotifyActivityInterrupted(FSovCoActionTestAccess::Goal(*F.Companion));
	TestEqual(TEXT("Interruption defers removal outside Narrative's selection stack"), F.Companion->GetCommandState(), ESovCompanionCommandState::MovingToAnchor);
	FSovCoActionTestAccess::Tick(*F.Companion);
	TestEqual(TEXT("Next tick fails interrupted action"), F.Companion->GetCommandState(), ESovCompanionCommandState::Failed);
	TestTrue(TEXT("Can retry interrupted action"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	FSovCoActionTestAccess::Expire(*F.Companion); FSovCoActionTestAccess::Tick(*F.Companion);
	TestEqual(TEXT("Timeout without valid hidden navigation fallback fails"), F.Companion->GetCommandState(), ESovCompanionCommandState::Failed);
	TestFalse(TEXT("Timeout cannot complete mission"), F.PC->State->IsBeatComplete(F.Mission->MissionId, TEXT("Arrival")));
	auto* Encounter = F.World->SpawnActor<ASovEncounterRuntimeTestDirector>(); Encounter->SeedState(ESovEncounterState::Active);
	F.Companion->RequiredEncounter = Encounter;
	TestTrue(TEXT("Retry before defeat"), F.Companion->RequestCoAction(F.Player, F.Anchor, Reason));
	FSovCoActionTestAccess::Death(*F.Companion);
	TestEqual(TEXT("Required companion defeat fails encounter"), Encounter->GetEncounterState(), ESovEncounterState::Failed);
	TestEqual(TEXT("Defeat releases action"), F.Companion->GetCommandState(), ESovCompanionCommandState::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCoActionFallbackVisibilityTest, "ProjectVelkorran.Campaign.Companion.HiddenFallbackVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCoActionFallbackVisibilityTest::RunTest(const FString& Parameters)
{
	FCoActionWorld F; if (!TestTrue(TEXT("World fixture"), F.Ready())) { return false; }
	if (!F.PC->PlayerCameraManager) { F.PC->PlayerCameraManager = F.World->SpawnActor<APlayerCameraManager>(); }
	F.PC->SetViewTarget(F.Player); F.Player->SetActorLocation(FVector(-600.f, 0.f, 0.f));
	F.NPC->SetActorLocation(FVector(0.f, 0.f, 96.f));
	const FVector Destination(150.f, 50.f, 96.f);
	TestFalse(TEXT("Visible companion cannot be teleported"), FSovCoActionTestAccess::Hidden(*F.Companion, Destination));
	AActor* Wall = F.Wall(FVector(-200.f, 0.f, 0.f));
	TestTrue(TEXT("Both complete capsules occluded from the player camera"), FSovCoActionTestAccess::Hidden(*F.Companion, Destination));
	Wall->Destroy();
	TestFalse(TEXT("Removing occlusion immediately invalidates fallback"), FSovCoActionTestAccess::Hidden(*F.Companion, Destination));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionFocusPursuitRepairTest, "ProjectVelkorran.Campaign.Companion.FocusPursuesTargetWithinLeaderLeash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionFocusPursuitRepairTest::RunTest(const FString& Parameters)
{
	FCoActionWorld F; if (!TestTrue(TEXT("World fixture"), F.Ready())) { return false; }
	F.Mission->AllowedCompanionIds.Add(F.Companion->CompanionId);
	auto* ASC = F.NPC->GetNarrativeAbilitySystemComponent();
	FSovCoActionTestAccess::SeedLeader(*F.Companion, F.Player, ASC);
	F.Companion->CuratedAbilities.Add(USovBotTestAttackAlpha::StaticClass());
	ASC->GiveAbility(FGameplayAbilitySpec(USovBotTestAttackAlpha::StaticClass(), 1));
	auto* Target = F.World->SpawnActor<ASovCoActionTestNPC>();
	if (!TestNotNull(TEXT("Living hostile target"), Target)) { return false; }
	Target->InitializeTestCombat(1); Target->SetActorLocation(FVector(1500.f, 0.f, 0.f));
	F.Player->SetActorLocation(FVector(0.f, 100.f, 0.f)); F.AI->bCaptureCommandMoves = true;
	FString Error;
	if (!TestTrue(TEXT("Real command permission admits target in leader's 25m radius"),
		F.Companion->RequestCommand(F.Player, ESovCompanionCommand::FocusTarget, Target, Error))) { AddError(Error); return false; }
	auto* Goal = FSovCoActionTestAccess::CommandGoal(*F.Companion);
	F.Companion->TickContextCommand(Goal);
	TestEqual(TEXT("Native owner issues one bounded path request"), F.AI->CapturedMoveCount, 1);
	TestTrue(TEXT("Path request follows selected enemy rather than nearby leader"), F.AI->CapturedMoveTarget.Get() == Target);
	TestTrue(TEXT("Approach acceptance lies inside the curated attack's range"), F.AI->CapturedAcceptanceRadius > 0.f && F.AI->CapturedAcceptanceRadius < 1000.f);
	F.Companion->TickContextCommand(Goal);
	TestEqual(TEXT("Failed navigation retries at bounded cadence"), F.AI->CapturedMoveCount, 1);
	Target->SetActorLocation(FVector(3000.f, 0.f, 0.f)); F.Companion->TickContextCommand(Goal);
	TestFalse(TEXT("Target escaping the leader leash retires its command"), F.Companion->IsCommandCurrent(Goal));
	TestEqual(TEXT("Stale command cannot issue pursuit after cancellation"), F.AI->CapturedMoveCount, 1);
	return true;
}
#endif
