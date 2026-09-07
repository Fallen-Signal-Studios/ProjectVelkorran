// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionMissionDefinition.h"
#include "Campaign/SovEvidenceDefinition.h"
#include "Character/PlayerDefinition.h"
#include "AI/NPCDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	FSovCampaignBeatDefinition* MutableBeat(USovCampaignDefinition& Mission, FName Id)
	{ return Mission.Beats.FindByPredicate([Id](const auto& Beat) { return Beat.BeatId == Id; }); }

	/** In-memory structural fixtures only. They do not stand in for authored scene playback or engine content qualification. */
	void BindStructuralFixtures(USovAurelionMissionDefinition& Mission, TArray<TStrongObjectPtr<UObject>>& KeepAlive)
	{
		auto* Player = NewObject<UPlayerDefinition>(); KeepAlive.Emplace(Player); Mission.PlayerDefinition = Player;
		for (auto& Profile : Mission.AlternateProtagonists)
		{ auto* Other = NewObject<UPlayerDefinition>(); KeepAlive.Emplace(Other); Profile.PlayerDefinition = Other; }
		for (auto& Profile : Mission.ProtagonistCompanions)
		{ auto* NPC = NewObject<UNPCDefinition>(); KeepAlive.Emplace(NPC); Profile.CompanionDefinition = NPC; }
		for (auto& Beat : Mission.Beats)
		for (int32 Index = 0; Index < Beat.CriticalEvidence.Num(); ++Index)
		{
			auto* Evidence = NewObject<USovEvidenceDefinition>(); KeepAlive.Emplace(Evidence);
			Evidence->EvidenceId = FName(*(Beat.BeatId.ToString() + FString::FromInt(Index)));
			Evidence->CanonicalContentId = Evidence->EvidenceId; Evidence->Summary = FText::FromString(TEXT("Structural test fixture."));
			Evidence->OriginalCustodian = TEXT("FixtureWitness"); Evidence->SourceCustodians.Add(Evidence->OriginalCustodian);
			Evidence->RelevantMissions.Add(Mission.MissionId); Evidence->bCriticalPath = true;
			Beat.CriticalEvidence[Index] = Evidence;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionNativeStructure,
	"ProjectVelkorran.Campaign.Aurelion.NativeStructureAndContentGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionNativeStructure::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USovAurelionTarrikPreparationMissionDefinition> Preparation(NewObject<USovAurelionTarrikPreparationMissionDefinition>());
	TStrongObjectPtr<USovAurelionFireAndFrostMissionDefinition> M12(NewObject<USovAurelionFireAndFrostMissionDefinition>());
	TStrongObjectPtr<USovAurelionContraryWitnessMissionDefinition> M13(NewObject<USovAurelionContraryWitnessMissionDefinition>());
	TArray<TStrongObjectPtr<UObject>> KeepAlive;
	FString Error;
	for (USovAurelionMissionDefinition* Mission : {static_cast<USovAurelionMissionDefinition*>(Preparation.Get()),
		static_cast<USovAurelionMissionDefinition*>(M12.Get()), static_cast<USovAurelionMissionDefinition*>(M13.Get())})
	{
		TestTrue(*FString::Printf(TEXT("%s source contract validates before content exists"), *Mission->MissionId.ToString()), Mission->ValidateAurelionContract(Error));
		USovCampaignDefinition* Base = Mission;
		TestFalse(TEXT("Existing base-pointer admission cannot mistake an unassigned scaffold for playable content"), Base->ValidateDefinition(Error));
		TestTrue(TEXT("Missing map has an actionable diagnostic"), Error.Contains(TEXT("authored map")));
		BindStructuralFixtures(*Mission, KeepAlive);
		// Qualifying the call deliberately isolates existing DAG/lead/companion validation from missing authored scenes.
		const bool bDAGValid = Mission->USovCampaignDefinition::ValidateDefinition(Error);
		TestTrue(*FString::Printf(TEXT("%s existing DAG, choice and handoff validation: %s"), *Mission->MissionId.ToString(), *Error), bDAGValid);
	}
	TestTrue(TEXT("Preparation has no successor into canon"), Preparation->AllowedSuccessorMissions.IsEmpty());
	TestFalse(TEXT("Preparation cannot enable companion Resonance"), Preparation->bAllowJointResonance);
	TestFalse(TEXT("M13 never authorizes the terminal release Resonance action"), M13->AllowedResonanceTypes.Contains(ESovResonanceType::TerminalRelease));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionCanonMutation,
	"ProjectVelkorran.Campaign.Aurelion.CanonMutationFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionCanonMutation::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USovAurelionContraryWitnessMissionDefinition> Mission(NewObject<USovAurelionContraryWitnessMissionDefinition>());
	FString Error;
	auto* Containment = MutableBeat(*Mission, TEXT("MeridianContainment"));
	if (!TestNotNull(TEXT("Containment contract exists"), Containment)) { return false; }
	const auto Original = *Containment;
	Containment->bRequiresCinematicProof = false;
	TestFalse(TEXT("Generic terminal cannot replace containment presentation proof"), Mission->ValidateAurelionContract(Error));
	*Containment = Original;
	for (auto& Fact : Containment->StateWrites)
	{
		TestTrue(TEXT("Protected M13 facts are historical milestones so later death/containment loss remains possible"),
			Fact.Key.ToString().StartsWith(TEXT("Campaign.Aurelion.M13.Fact.")));
		const auto Saved = Fact;
		Fact.bCanonProtected = false;
		TestFalse(TEXT("Each required canon observation keeps its protection"), Mission->ValidateAurelionContract(Error));
		Fact = Saved;
		Fact.Value = FSovGameplayTags::Get().Character_Player_Tarrik;
		TestFalse(TEXT("Release, boundary, Lyric and terminal state values cannot be substituted"), Mission->ValidateAurelionContract(Error));
		Fact = Saved;
	}
	auto* Choice = MutableBeat(*Mission, TEXT("ProtectMedicalGroup"));
	if (!TestNotNull(TEXT("Local protection choice exists"), Choice)) { return false; }
	Choice->StateWrites.Add(Containment->StateWrites[0]);
	TestFalse(TEXT("Optional survivor outcome cannot produce a protected containment fact"), Mission->ValidateAurelionContract(Error));
	Choice->StateWrites.Reset();
	Mission->AllowedResonanceTypes.Add(ESovResonanceType::TerminalRelease);
	TestFalse(TEXT("Terminal authority cannot be expanded into a prison-release action"), Mission->ValidateAurelionContract(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionAssentBoundaries,
	"ProjectVelkorran.Campaign.Aurelion.IndependentAssentAndEvidenceOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionAssentBoundaries::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USovAurelionContraryWitnessMissionDefinition> Mission(NewObject<USovAurelionContraryWitnessMissionDefinition>());
	FString Error; const auto& Tags = FSovGameplayTags::Get();
	auto* Selene = MutableBeat(*Mission, TEXT("SeleneIndependentAssent"));
	auto* Tarrik = MutableBeat(*Mission, TEXT("TarrikIndependentAssent"));
	auto* Containment = MutableBeat(*Mission, TEXT("MeridianContainment"));
	if (!Selene || !Tarrik || !Containment) { AddError(TEXT("Required assent contracts missing.")); return false; }
	const auto OriginalSelene = *Selene;
	Selene->RequiredProtagonist = Tags.Character_Player_Tarrik;
	TestFalse(TEXT("Tarrik cannot speak Selene's assent"), Mission->ValidateAurelionContract(Error));
	*Selene = OriginalSelene;
	Selene->bRequiresCinematicProof = false; Selene->bRequiresCoActionProof = true;
	Selene->RequiredCompanionId = TEXT("Tarrik"); Selene->RequiredCoActionAnchorId = TEXT("M13_TarrikContraryPosition");
	TestFalse(TEXT("Companion arrival cannot substitute for independent assent"), Mission->ValidateAurelionContract(Error));
	*Selene = OriginalSelene;
	Containment->PrerequisiteBeats.Remove(TEXT("SeleneIndependentAssent"));
	TestFalse(TEXT("Meridian completion explicitly retains both independent assents"), Mission->ValidateAurelionContract(Error));
	Containment->PrerequisiteBeats.Add(TEXT("SeleneIndependentAssent"));
	auto* Record = MutableBeat(*Mission, TEXT("Record7283Received"));
	auto* Recorder = MutableBeat(*Mission, TEXT("CauldronRecorderReceived"));
	if (!Record || !Recorder) { AddError(TEXT("Evidence exchange contracts missing.")); return false; }
	TestEqual(TEXT("Record 7283 is acquired while controlling Tarrik"), Record->RequiredProtagonist, Tags.Character_Player_Tarrik);
	TestEqual(TEXT("The Cauldron recorder is acquired while controlling Selene"), Recorder->RequiredProtagonist, Tags.Character_Player_Selene);
	Recorder->RequiredProtagonist = Tags.Character_Player_Tarrik;
	TestFalse(TEXT("Evidence recipient cannot be reassigned by changing the active perspective"), Mission->ValidateAurelionContract(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionPreparationAdmission,
	"ProjectVelkorran.Campaign.Aurelion.PreparationRequiresRealEncounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionPreparationAdmission::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
	if (!World) { AddError(TEXT("Could not create campaign test world.")); return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	auto* PC = World->SpawnActor<ASovCampaignRuntimeTestController>();
	auto* Pawn = World->SpawnActor<ASovCampaignRuntimeTestPawn>();
	if (!PC || !Pawn)
	{
		World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
		AddError(TEXT("Could not create possessed campaign fixtures.")); return false;
	}
	PC->Possess(Pawn);
	auto* Mission = NewObject<USovAurelionTarrikPreparationMissionDefinition>(PC);
	auto* Player = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Mission); PC->KeepAlive.Add(Player);
	Mission->PlayerDefinition = Player; Mission->PawnClass = ASovCampaignRuntimeTestPawn::StaticClass(); Mission->Map = World;
	FString Error;
	TestTrue(TEXT("Preparation admits real bound map/player data"), Mission->ValidateDefinition(Error));
	TestEqual(TEXT("Existing campaign state begins the bounded preparation mission"), PC->State->BeginMission(Mission), ESovCampaignResult::Applied);
	TestTrue(TEXT("Route cannot complete before the encounter"), PC->State->CompleteBeat(TEXT("SecureTarrikRoute")) != ESovCampaignResult::Applied);
	TestEqual(TEXT("Arrival commits normally"), PC->State->CompleteBeat(TEXT("TarrikArrival")), ESovCampaignResult::Applied);
	TestTrue(TEXT("A generic terminal cannot claim encounter victory"), PC->State->CompleteBeat(TEXT("HoldMixedSurvivorCorridor")) != ESovCampaignResult::Applied);
	TestTrue(TEXT("A skip request cannot claim encounter victory"), PC->State->CompleteBeat(TEXT("HoldMixedSurvivorCorridor"), true) != ESovCampaignResult::Applied);
	TestFalse(TEXT("Rejected claims do not finish the mission"), PC->State->IsMissionComplete(Mission->MissionId));
	TestEqual(TEXT("Only the actual arrival reaches the journal"), PC->State->GetJournal().Num(), 1);
	USovCampaignDefinition* Base = Mission;
	auto* Hold = MutableBeat(*Mission, TEXT("HoldMixedSurvivorCorridor"));
	TestEqual(TEXT("Mixed-survivor protection requires at least two protected identities"), Hold->MinimumProtectedParticipants, 2);
	Hold->MinimumProtectedParticipants = 0;
	TestFalse(TEXT("Removing survivor protection cannot reduce this objective to ordinary elimination"), Base->ValidateDefinition(Error));
	Hold->MinimumProtectedParticipants = -1;
	TestFalse(TEXT("Base validation rejects a negative protected-participant contract"), Mission->USovCampaignDefinition::ValidateDefinition(Error));
	Hold->MinimumProtectedParticipants = 2;
	Hold->RequiredEncounterId = NAME_None;
	TestFalse(TEXT("Base validation rejects participant proof without an encounter owner"), Mission->USovCampaignDefinition::ValidateDefinition(Error));
	TestFalse(TEXT("Existing base-pointer validation notices a removed proof requirement"), Base->ValidateDefinition(Error));
	TestTrue(TEXT("Runtime refuses the edited contract before any write"), PC->State->CompleteBeat(TEXT("HoldMixedSurvivorCorridor")) != ESovCampaignResult::Applied);
	TestEqual(TEXT("Contract tampering produces no new journal event"), PC->State->GetJournal().Num(), 1);
	World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionSceneBinding,
	"ProjectVelkorran.Campaign.Aurelion.StorySequenceCannotBeSubstituted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionSceneBinding::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USovAurelionContraryWitnessMissionDefinition> Mission(NewObject<USovAurelionContraryWitnessMissionDefinition>());
	// Identity-only fixtures: loadability, tracks, participants and playback receipts remain the native cinematic owner's job.
	const TSoftObjectPtr<ULevelSequence> Fifth(FSoftObjectPath(TEXT("/Game/AurelionTests/LS_Fifth.LS_Fifth")));
	const TSoftObjectPtr<ULevelSequence> Assent(FSoftObjectPath(TEXT("/Game/AurelionTests/LS_Assent.LS_Assent")));
	TestFalse(TEXT("An unassigned dependency cannot admit any candidate"), Mission->MatchesStorySequence(TEXT("FifthWitness"), Fifth));
	Mission->StorySequences.Add(TEXT("M13_FifthWitness"), Fifth);
	Mission->StorySequences.Add(TEXT("M13_SeleneIndependentAssent"), Assent);
	TestTrue(TEXT("The declared scene admits only its own beat"), Mission->MatchesStorySequence(TEXT("FifthWitness"), Fifth));
	TestFalse(TEXT("A valid different scene cannot manufacture Fifth Witness proof"), Mission->MatchesStorySequence(TEXT("FifthWitness"), Assent));
	TestFalse(TEXT("A scene cannot complete an encounter objective"), Mission->MatchesStorySequence(TEXT("EclipseEscalation"), Fifth));
	TestFalse(TEXT("Unknown beat is rejected"), Mission->MatchesStorySequence(TEXT("InventedBeat"), Fifth));
	Mission->StorySequences[TEXT("M13_FifthWitness")] = Assent;
	TestFalse(TEXT("Changing the declared asset invalidates the previously admitted scene before commit"), Mission->MatchesStorySequence(TEXT("FifthWitness"), Fifth));
	return true;
}
#endif
