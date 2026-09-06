// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
struct FSovObjectiveLifecycleTestAccess
{
	static void CorruptState(USovCampaignStateComponent& State, FName Mission, FName Beat)
	{ State.Missions.FindChecked(Mission).ObjectiveStates.Add(Beat, ESovObjectiveState::Succeeded); }
	static void CorruptChoice(USovCampaignStateComponent& State, FName Mission, FName Outcome)
	{ State.Missions.FindChecked(Mission).SelectedChoices.Add(TEXT("ProtectionPriority"), Outcome); }
	static void CorruptObjectiveOrder(USovCampaignStateComponent& State) { State.ObjectiveJournal[0].AfterBeatSequence = 9999; }
	static void CorruptEvidenceOrder(USovCampaignStateComponent& State) { State.ObjectiveJournal.Last().AfterEvidenceCount = 0; }
	static void MakeLegacy(USovCampaignStateComponent& State)
	{
		State.SavedSchemaVersion = 1; State.ObjectiveJournal.Reset();
		for (auto& Pair : State.Missions) { Pair.Value.ObjectiveStates.Reset(); Pair.Value.SelectedChoices.Reset(); }
	}
};

namespace
{
	struct FObjectiveWorld
	{
		UWorld* World = nullptr;
		ASovCampaignRuntimeTestController* PC = nullptr;
		ASovCampaignRuntimeTestPawn* Pawn = nullptr;
		FObjectiveWorld()
		{
			const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<ASovCampaignRuntimeTestController>(); Pawn = World->SpawnActor<ASovCampaignRuntimeTestPawn>();
			if (PC && Pawn) { PC->Possess(Pawn); PC->State->OnBeatCommitted.AddUniqueDynamic(PC, &ASovCampaignRuntimeTestController::ObserveBeat); }
		}
		~FObjectiveWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
		USovCampaignDefinition* Mission(bool WithChoice = false)
		{
			auto* Definition = NewObject<USovCampaignDefinition>(PC); auto* Player = NewObject<UPlayerDefinition>(PC);
			PC->KeepAlive.Add(Definition); PC->KeepAlive.Add(Player);
			Definition->MissionId = TEXT("M12_ObjectiveProof"); Definition->Protagonist = Pawn->TestHero;
			Definition->PawnClass = ASovCampaignRuntimeTestPawn::StaticClass(); Definition->PlayerDefinition = Player;
			FSovCampaignBeatDefinition Start; Start.BeatId = TEXT("SurvivorsReached"); Definition->Beats.Add(Start);
			FSovCampaignBeatDefinition Rescue; Rescue.BeatId = TEXT("OptionalRescue"); Rescue.bOptional = true;
			Rescue.ObjectiveType = ESovObjectiveType::MasteryRescue; Rescue.PrerequisiteBeats = { Start.BeatId };
			Rescue.FailureReasonId = TEXT("RescueWindowExpired"); Rescue.FailureRuleText = FText::FromString(TEXT("Reach the survivor before the rescue window closes."));
			Definition->Beats.Add(Rescue);
			FSovCampaignBeatDefinition Escape; Escape.BeatId = TEXT("EscapeTogether"); Escape.bCanonGate = true; Escape.PrerequisiteBeats = { Start.BeatId };
			if (WithChoice)
			{
				FSovCampaignChoiceGroup Group; Group.GroupId = TEXT("ProtectionPriority"); Group.ReconciliationBeatId = Escape.BeatId;
				Group.ReconciliationNote = FText::FromString(TEXT("Both priorities change the rescued crew acknowledged during escape; Tarrik and Selene still escape together."));
				Definition->ChoiceGroups.Add(Group); Escape.RequiredChoiceGroups.Add(Group.GroupId);
				for (const FName Outcome : { FName(TEXT("ProtectDominionCrew")), FName(TEXT("ProtectReformationCrew")) })
				{
					FSovCampaignBeatDefinition Choice; Choice.BeatId = Outcome; Choice.PrerequisiteBeats = { Start.BeatId };
					Choice.bOptional = true; Choice.bInteractiveChoice = true; Choice.ChoiceGroupId = Group.GroupId;
					Choice.ObjectiveType = ESovObjectiveType::ChoosePrioritize;
					FSovConsequenceDefinition Fact; Fact.ConsequenceId = Outcome; Fact.SubjectIds = { Outcome };
					Fact.ChoiceTag = FSovGameplayTags::Get().Character_Player_Tarrik; Fact.OutcomeTag = FSovGameplayTags::Get().Character_Player_Selene;
					Fact.ConsumerIds = { Escape.BeatId }; Choice.Consequences.Add(Fact); Definition->Beats.Add(Choice);
				}
			}
			Definition->Beats.Add(Escape);
			return Definition;
		}
		TArray<uint8> Save(USovCampaignStateComponent* State)
		{
			TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer, false); Archive.ArIsSaveGame = true; Archive.ArNoDelta = true;
			State->Serialize(Archive); return Archive.IsError() ? TArray<uint8>() : Bytes;
		}
		USovCampaignStateComponent* Restore(const TArray<uint8>& Bytes)
		{
			auto* Restored = NewObject<USovCampaignStateComponent>(PC); PC->KeepAlive.Add(Restored);
			FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Archive(Reader, true); Archive.ArIsSaveGame = true;
			Restored->Serialize(Archive); if (Archive.IsError()) { return nullptr; }
			Restored->Load_Implementation(); return Restored;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveLifecycleRuntimeTest, "ProjectVelkorran.Campaign.Objectives.TerminalLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveLifecycleRuntimeTest::RunTest(const FString& Parameters)
{
	FObjectiveWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); auto* State = F.PC->State.Get();
	TestEqual(TEXT("Begin"), State->BeginMission(Mission), ESovCampaignResult::Applied);
	TestEqual(TEXT("Future rescue is hidden"), State->GetObjectiveState(Mission->MissionId, TEXT("OptionalRescue")), ESovObjectiveState::Inactive);
	TestEqual(TEXT("Cannot activate future rescue"), State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Active), ESovCampaignResult::PrerequisiteMissing);
	TestEqual(TEXT("Start"), State->CompleteBeat(TEXT("SurvivorsReached")), ESovCampaignResult::Applied);
	TestEqual(TEXT("Rescue becomes available"), State->GetObjectiveState(Mission->MissionId, TEXT("OptionalRescue")), ESovObjectiveState::Available);
	TestEqual(TEXT("Activate"), State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Active), ESovCampaignResult::Applied);
	TestEqual(TEXT("Activation retry is idempotent"), State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Active), ESovCampaignResult::AlreadyApplied);
	TestEqual(TEXT("Authored failure"), State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Failed), ESovCampaignResult::Applied);
	TestEqual(TEXT("Failure cannot later grant success"), State->CompleteBeat(TEXT("OptionalRescue")), ESovCampaignResult::ObjectiveClosed);
	TestFalse(TEXT("Failed rescue leaves actionable goals"), State->GetActionableObjectiveIds().Contains(TEXT("OptionalRescue")));
	TestEqual(TEXT("Canon cannot skip"), State->TransitionObjective(TEXT("EscapeTogether"), ESovObjectiveState::Skipped), ESovCampaignResult::ObjectiveClosed);
	TestEqual(TEXT("Canon cannot fail"), State->TransitionObjective(TEXT("EscapeTogether"), ESovObjectiveState::Failed), ESovCampaignResult::ObjectiveClosed);
	TestEqual(TEXT("Canon remains reachable after optional failure"), State->CompleteBeat(TEXT("EscapeTogether")), ESovCampaignResult::Applied);
	TestTrue(TEXT("Mission succeeded"), State->IsMissionComplete(Mission->MissionId));
	TestEqual(TEXT("Only activation and failure journaled"), State->GetObjectiveJournal().Num(), 2);
	auto* Reloaded = F.Restore(F.Save(State));
	TestNotNull(TEXT("Serialized restore"), Reloaded);
	if (!Reloaded) { return false; }
	TestTrue(TEXT("Ordered objective journal validates"), Reloaded->IsStateValid());
	TestEqual(TEXT("Failed state survives reload"), Reloaded->GetObjectiveState(Mission->MissionId, TEXT("OptionalRescue")), ESovObjectiveState::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveChoiceRuntimeTest, "ProjectVelkorran.Campaign.Objectives.ExclusiveProtectionChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveChoiceRuntimeTest::RunTest(const FString& Parameters)
{
	for (const FName Selected : { FName(TEXT("ProtectDominionCrew")), FName(TEXT("ProtectReformationCrew")) })
	{
		FObjectiveWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
		auto* Mission = F.Mission(true); auto* State = F.PC->State.Get(); FString Error;
		const FName Other = Selected == TEXT("ProtectDominionCrew") ? FName(TEXT("ProtectReformationCrew")) : FName(TEXT("ProtectDominionCrew"));
		TestTrue(TEXT("Authored bounded choice validates"), Mission->ValidateDefinition(Error));
		State->BeginMission(Mission); State->CompleteBeat(TEXT("SurvivorsReached"));
		TestEqual(TEXT("Reconciliation waits for one outcome"), State->CompleteBeat(TEXT("EscapeTogether")), ESovCampaignResult::PrerequisiteMissing);
		TestEqual(TEXT("Choice cannot be skipped out of existence"), State->TransitionObjective(Selected, ESovObjectiveState::Skipped), ESovCampaignResult::ObjectiveClosed);
		F.PC->ReentrantBeat = Other;
		TestEqual(TEXT("Selected outcome commits"), State->ResolveChoice(TEXT("ProtectionPriority"), Selected), ESovCampaignResult::Applied);
		TestEqual(TEXT("Reentrant alternative blocked"), F.PC->NestedResult, ESovCampaignResult::Busy); F.PC->ReentrantBeat = NAME_None;
		TestEqual(TEXT("Selected outcome retry is idempotent"), State->ResolveChoice(TEXT("ProtectionPriority"), Selected), ESovCampaignResult::AlreadyApplied);
		TestEqual(TEXT("Generic beat API cannot commit alternative"), State->CompleteBeat(Other), ESovCampaignResult::ObjectiveClosed);
		TestEqual(TEXT("Alternative superseded"), State->GetObjectiveState(Mission->MissionId, Other), ESovObjectiveState::Superseded);
		TestFalse(TEXT("Alternative leaves actionable goals"), State->GetActionableObjectiveIds().Contains(Other));
		FSovConsequenceRecord Fact;
		TestTrue(TEXT("Selected protection consequence exists"), State->FindConsequence(Selected, NAME_None, Fact));
		TestFalse(TEXT("Alternative consequence never granted"), State->FindConsequence(Other, NAME_None, Fact));
		auto* Reloaded = F.Restore(F.Save(State));
		if (!TestNotNull(TEXT("Choice snapshot reload"), Reloaded)) { return false; }
		TestTrue(TEXT("Choice replay validates"), Reloaded->IsStateValid());
		TestEqual(TEXT("Choice selection survives"), Reloaded->GetSelectedChoice(Mission->MissionId, TEXT("ProtectionPriority")), Selected);
		TestEqual(TEXT("Both legal choices rejoin fixed escape"), Reloaded->CompleteBeat(TEXT("EscapeTogether")), ESovCampaignResult::Applied);
		TestTrue(TEXT("Both legal branches complete mission"), Reloaded->IsMissionComplete(Mission->MissionId));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveTamperRuntimeTest, "ProjectVelkorran.Campaign.Objectives.ReplayRejectsTampering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveTamperRuntimeTest::RunTest(const FString& Parameters)
{
	FObjectiveWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(true); auto* State = F.PC->State.Get();
	State->BeginMission(Mission); State->CompleteBeat(TEXT("SurvivorsReached"));
	State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Skipped);
	State->ResolveChoice(TEXT("ProtectionPriority"), TEXT("ProtectDominionCrew"));
	const auto Bytes = F.Save(State);
	FString Error; TestTrue(TEXT("Save preflight validates"), USovCampaignStateComponent::ValidateSerializedSave(Bytes, Error));
	auto* Copy = F.Restore(Bytes); if (!Copy) { return false; }
	FSovObjectiveLifecycleTestAccess::CorruptState(*Copy, Mission->MissionId, TEXT("OptionalRescue")); Copy->Load_Implementation();
	TestFalse(TEXT("Forged success cache rejected"), Copy->IsStateValid());
	Copy = F.Restore(Bytes); if (!Copy) { return false; }
	FSovObjectiveLifecycleTestAccess::CorruptChoice(*Copy, Mission->MissionId, TEXT("ProtectReformationCrew")); Copy->Load_Implementation();
	TestFalse(TEXT("Swapped chosen alternative rejected"), Copy->IsStateValid());
	Copy = F.Restore(Bytes); if (!Copy) { return false; }
	FSovObjectiveLifecycleTestAccess::CorruptObjectiveOrder(*Copy); Copy->Load_Implementation();
	TestFalse(TEXT("Reordered objective event rejected"), Copy->IsStateValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveMigrationRuntimeTest, "ProjectVelkorran.Campaign.Objectives.LegacyCompletionMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveMigrationRuntimeTest::RunTest(const FString& Parameters)
{
	FObjectiveWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); auto* State = F.PC->State.Get();
	State->BeginMission(Mission); State->CompleteBeat(TEXT("SurvivorsReached"));
	FSovObjectiveLifecycleTestAccess::MakeLegacy(*State);
	const auto Bytes = F.Save(State); FString Error;
	TestTrue(TEXT("Schema 1 preflight migrates before validating"), USovCampaignStateComponent::ValidateSerializedSave(Bytes, Error));
	auto* Reloaded = F.Restore(Bytes); if (!TestNotNull(TEXT("Legacy snapshot reload"), Reloaded)) { return false; }
	TestTrue(TEXT("Migrated state valid"), Reloaded->IsStateValid());
	TestEqual(TEXT("Only completed legacy beat succeeds"), Reloaded->GetObjectiveState(Mission->MissionId, TEXT("SurvivorsReached")), ESovObjectiveState::Succeeded);
	TestEqual(TEXT("Unresolved rescue remains available"), Reloaded->GetObjectiveState(Mission->MissionId, TEXT("OptionalRescue")), ESovObjectiveState::Available);
	TestTrue(TEXT("Migration invents no transition journal"), Reloaded->GetObjectiveJournal().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveDefinitionRuntimeTest, "ProjectVelkorran.Campaign.Objectives.DefinitionCanonSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveDefinitionRuntimeTest::RunTest(const FString& Parameters)
{
	FObjectiveWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(true); FString Error;
	Mission->ChoiceGroups[0].ReconciliationNote = FText();
	TestFalse(TEXT("Missing canon reconciliation rejected"), Mission->ValidateDefinition(Error));
	Mission->ChoiceGroups[0].ReconciliationNote = FText::FromString(TEXT("Both crews escape through the fixed gate."));
	Mission->Beats.Last().RequiredChoiceGroups.Reset();
	TestFalse(TEXT("Mandatory route cannot bypass unresolved choice"), Mission->ValidateDefinition(Error));
	Mission->Beats.Last().RequiredChoiceGroups = { TEXT("ProtectionPriority") };
	Mission->Beats[2].FailureReasonId = TEXT("Timeout"); Mission->Beats[2].FailureRuleText = FText::FromString(TEXT("Timeout"));
	TestFalse(TEXT("Choice outcomes cannot fail and strand reconciliation"), Mission->ValidateDefinition(Error));
	Mission->Beats[2].FailureReasonId = NAME_None; Mission->Beats[2].FailureRuleText = FText();
	Mission->Beats[2].GrantedKnowledge.AddTag(FSovGameplayTags::Get().Character_Player_Tarrik);
	TestFalse(TEXT("A unique outcome cannot become mandatory knowledge"), Mission->ValidateDefinition(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveEvidenceOrderingRuntimeTest, "ProjectVelkorran.Campaign.Objectives.EvidenceBeforeActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveEvidenceOrderingRuntimeTest::RunTest(const FString& Parameters)
{
	FObjectiveWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); auto* State = F.PC->State.Get();
	const auto Knowledge = FSovGameplayTags::Get().Echo_Source_WeakPointBreak;
	Mission->Beats[1].RequiredKnowledge.AddTag(Knowledge);
	State->BeginMission(Mission);
	TestEqual(TEXT("Objective activation before first completion"), State->TransitionObjective(TEXT("SurvivorsReached"), ESovObjectiveState::Active), ESovCampaignResult::Applied);
	State->CompleteBeat(TEXT("SurvivorsReached"));
	TestEqual(TEXT("Unknown rescue remains hidden"), State->GetObjectiveState(Mission->MissionId, TEXT("OptionalRescue")), ESovObjectiveState::Inactive);
	auto* Actor = F.World->SpawnActor<AActor>();
	if (!Actor) { AddError(TEXT("Evidence actor failed")); return false; }
	auto* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10.f)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldDynamic); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent(); Actor->SetActorLocation(FVector(100.f, 0.f, 0.f));
	auto* Source = NewObject<USovEvidenceSourceComponent>(Actor); Actor->AddInstanceComponent(Source); Source->RegisterComponent();
	Source->EvidenceId = TEXT("SurvivorLocation"); Source->SourceId = FGuid::NewGuid(); Source->AcquisitionMission = Mission->MissionId;
	Source->AllowedProtagonists.AddTag(F.Pawn->TestHero); Source->GrantedKnowledge.AddTag(Knowledge);
	TestEqual(TEXT("Evidence grants rescue knowledge"), State->AcquireEvidence(Source), ESovCampaignResult::Applied);
	TestEqual(TEXT("Activation after evidence accepted"), State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Active), ESovCampaignResult::Applied);
	auto* Reloaded = F.Restore(F.Save(State)); if (!TestNotNull(TEXT("Ordered reload"), Reloaded)) { return false; }
	TestTrue(TEXT("Evidence-ordered activation validates"), Reloaded->IsStateValid());
	FSovObjectiveLifecycleTestAccess::CorruptEvidenceOrder(*Reloaded); Reloaded->Load_Implementation();
	TestFalse(TEXT("Moving activation before required knowledge is rejected"), Reloaded->IsStateValid());
	return true;
}
#endif
