#include "Tests/SovNarrativeCueRuntimeFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/PlayerDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "Misc/AutomationTest.h"
#include "Narrative/SovNarrativeCueComponent.h"
#include "Tales/TalesComponent.h"
#include "Sovereign/SovGameplayTags.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundClass.h"
#include "UI/SovAccessibleRecordMenu.h"
#include <limits>

#if WITH_AUTOMATION_TESTS
struct FSovNarrativeCueTestAccess
{
	static void Tick(USovNarrativeCueComponent* C) { C->TickComponent(.05f, LEVELTICK_All, nullptr); }
	static USovNarrativeCue* Playing(USovNarrativeCueComponent* C) { return C->CurrentBark; }
	static int32 Queued(USovNarrativeCueComponent* C) { return C->Pending.Num(); }
	static void Interrupt(USovNarrativeCueComponent* C) { C->StopBark(true); }
	static void DuplicateInFlight(USovNarrativeCueComponent* C) { C->Pending.Add(C->InFlightCriticalSave); }
	static bool ControllerOutput(UAudioComponent* Audio, USoundClass* Class, float Volume)
	{ return USovNarrativeCueComponent::ConfigureControllerOutput(Audio, Class, Volume); }
	static bool ReviewContains(USovAccessibleRecordMenu* Menu, const FString& Text)
	{ return Menu->Records.ContainsByPredicate([&](const FText& Record) { return Record.ToString().Contains(Text); }); }
};
namespace
{
	struct FCueWorld
	{
		UWorld* World; ASovHandoffRuntimeTestController* PC; ASovHandoffRuntimeTestPawn* Pawn;
		FCueWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<ASovHandoffRuntimeTestController>(); Pawn = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
			auto* PS = World->SpawnActor<ASovPlayerState>(); auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
			Pawn->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Pawn);
			Pawn->StageTestReadiness(PS, true); Pawn->CompleteCampaignDataInitialization(false);
			auto* Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
			Mission->MissionId = TEXT("M01_CueTest"); Mission->Protagonist = Pawn->GetProtagonistIdentityTag();
			Mission->PawnClass = Pawn->GetClass(); Mission->PlayerDefinition = Definition;
			FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("Finish"); Mission->Beats.Add(Beat);
			PC->GetCampaignState()->BeginMission(Mission);
		}
		~FCueWorld() { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		USovNarrativeCue* Cue(FName Id, ESovNarrativeCuePriority Priority)
		{
			auto* Cue = NewObject<USovNarrativeCue>(PC); PC->KeepAlive.Add(Cue); Cue->CueId = Id; Cue->SpeakerId = TEXT("Tarrik");
			Cue->bPlayerSpeaker = true; Cue->Priority = Priority; FSovBarkVariant Variant; Variant.Caption = FText::FromString(TEXT("Test caption."));
			Cue->BarkVariants.Add(Variant); return Cue;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCuePriorityTest, "ProjectVelkorran.Campaign.Narrative.CuePriorityAndCriticalReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCuePriorityTest::RunTest(const FString& Parameters)
{
	FCueWorld F; auto* Cues = F.PC->GetNarrativeCues(); FString Error;
	auto* Ambient = F.Cue(TEXT("Ambient"), ESovNarrativeCuePriority::Ambient);
	auto* Critical = F.Cue(TEXT("Direction"), ESovNarrativeCuePriority::ObjectiveCritical);
	Critical->bCritical = true; Critical->bRecordUnheardSummary = true; Critical->RecordSummary = FText::FromString(TEXT("An authored record summary."));
	TestTrue(TEXT("Ambient context queues"), Cues->RequestCue(Ambient, F.Pawn, Error));
	FSovNarrativeCueTestAccess::Tick(Cues); TestTrue(TEXT("Real arbiter starts ambient caption"), FSovNarrativeCueTestAccess::Playing(Cues) == Ambient);
	TestTrue(TEXT("Higher priority critical direction queues"), Cues->RequestCue(Critical, F.Pawn, Error));
	FSovNarrativeCueTestAccess::Tick(Cues); TestTrue(TEXT("Critical direction replaces ambient, without overlap"), FSovNarrativeCueTestAccess::Playing(Cues) == Critical);
	TestFalse(TEXT("Same cue cannot duplicate while playing"), Cues->RequestCue(Critical, F.Pawn, Error));
	FSovNarrativeCueTestAccess::Interrupt(Cues);
	TestEqual(TEXT("Interrupted critical cue remains queued"), FSovNarrativeCueTestAccess::Queued(Cues), 1);
	TestTrue(TEXT("Only the explicitly diegetic summary is retained"), Cues->GetUnheardRecords().Contains(Critical));
	auto* Review = NewObject<USovAccessibleRecordMenu>(F.PC); Review->SetOwningPlayer(F.PC); Review->SetSceneHistoryMode(true);
	TestTrue(TEXT("Native records review consumes the existing saved unheard archive"), FSovNarrativeCueTestAccess::ReviewContains(Review, TEXT("An authored record summary.")));
	TestTrue(TEXT("Unheard information keeps its explicit presentation label"), FSovNarrativeCueTestAccess::ReviewContains(Review, TEXT("Unheard important record")));
	TestTrue(TEXT("Review does not falsely complete the original audio cue"), Cues->GetUnheardRecords().Contains(Critical));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueSuspensionTest, "ProjectVelkorran.Campaign.Narrative.DialogueSuspensionPreservesNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueSuspensionTest::RunTest(const FString& Parameters)
{
	FCueWorld F; auto* Tales = F.PC->FindComponentByClass<UTalesComponent>();
	auto* Dialogue = NewObject<USovNarrativeCueRuntimeDialogue>(F.PC); F.PC->KeepAlive.Add(Dialogue); Dialogue->Stage(Tales);
	UDialogueNode* Node = Dialogue->GetCurrentNode(); const int32 TasksBefore = Tales->MasterTaskList.Num();
	TestTrue(TEXT("Authored free movement graph can suspend"), Dialogue->SetPlaybackSuspended(true));
	TestTrue(TEXT("Actual Narrative line timer is paused"), Dialogue->IsNativeLineTimerPaused());
	TestFalse(TEXT("Paused graph cannot accept a choice"), Dialogue->CanSelectDialogueOption(Dialogue->GetTestChoice()));
	TestFalse(TEXT("Paused graph cannot send skip RPCs or finish a line"), Dialogue->CanSkipCurrentLine());
	Dialogue->FinishNPCLineForTest(); Dialogue->EndCurrentLine();
	TestTrue(TEXT("Late line callbacks retain current node"), Dialogue->GetCurrentNode() == Node);
	TestEqual(TEXT("Suspended callbacks do not complete Narrative tasks"), Tales->MasterTaskList.Num(), TasksBefore);
	TestTrue(TEXT("Graph resumes in place"), Dialogue->SetPlaybackSuspended(false));
	TestTrue(TEXT("Original line timer resumes"), Dialogue->IsNativeLineTimerActive());
	TestTrue(TEXT("Resuming retains selected context"), Dialogue->GetCurrentNode() == Node);
	TestTrue(TEXT("The same choice is available again"), Dialogue->CanSelectDialogueOption(Dialogue->GetTestChoice()));
	Dialogue->Deinitialize();
	TestFalse(TEXT("Ended graph cannot resume"), Dialogue->SetPlaybackSuspended(true));
	TestFalse(TEXT("Even an already-unpaused ended graph rejects a resume request"), Dialogue->SetPlaybackSuspended(false));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCueSaveIdentityTest, "ProjectVelkorran.Campaign.Narrative.CriticalSaveAndCallbackIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCueSaveIdentityTest::RunTest(const FString& Parameters)
{
	FCueWorld F; auto* Cues=F.PC->GetNarrativeCues(); FString Error;
	auto* Critical=F.Cue(TEXT("CriticalSave"),ESovNarrativeCuePriority::ObjectiveCritical);
	Critical->bCritical=true; Critical->bRecordUnheardSummary=true; Critical->RecordSummary=FText::FromString(TEXT("An authored record."));
	TestTrue(TEXT("Critical cue queues"),Cues->RequestCue(Critical,F.Pawn,Error)); FSovNarrativeCueTestAccess::Tick(Cues);
	Cues->PrepareForSave_Implementation(); FSovNarrativeCueTestAccess::DuplicateInFlight(Cues); Cues->Load_Implementation();
	TestEqual(TEXT("In-flight and queued copies restore one request"),FSovNarrativeCueTestAccess::Queued(Cues),1);
	TestTrue(TEXT("Loaded state does not retain old audio ownership"),FSovNarrativeCueTestAccess::Playing(Cues)==nullptr);
	TestEqual(TEXT("Stopping old playback during load does not contaminate loaded records"),Cues->GetUnheardRecords().Num(),0);
	auto* Observer=NewObject<USovNarrativeCueRuntimeObserver>(F.PC); F.PC->KeepAlive.Add(Observer); Observer->Cues=Cues;
	Cues->OnCueStarted.AddDynamic(Observer,&USovNarrativeCueRuntimeObserver::LoadDuringCueStart);
	FSovNarrativeCueTestAccess::Tick(Cues);
	TestEqual(TEXT("The old start callback cannot append its request after a load"),FSovNarrativeCueTestAccess::Queued(Cues),0);
	TestTrue(TEXT("Callback load leaves no old active bark"),FSovNarrativeCueTestAccess::Playing(Cues)==nullptr);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueCompletionOwnershipTest, "ProjectVelkorran.Campaign.Narrative.DialogueCompletionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueCompletionOwnershipTest::RunTest(const FString& Parameters)
{
	FCueWorld F; auto* Tales=F.PC->FindComponentByClass<UTalesComponent>();
	auto* Dialogue=NewObject<USovNarrativeCueRuntimeDialogue>(F.PC); F.PC->KeepAlive.Add(Dialogue); Dialogue->Stage(Tales);
	Dialogue->bReenterFinish=true; Dialogue->FinishNPCLineForTest();
	const int32 TasksAfter=Tales->MasterTaskList.Num(); Dialogue->FinishNPCLineForTest(); Dialogue->EndCurrentLine();
	TestEqual(TEXT("Recursive audio/finish callbacks complete the actual line once"),Dialogue->FinishNotifications,1);
	TestEqual(TEXT("Late callbacks do not replay Narrative tasks"),Tales->MasterTaskList.Num(),TasksAfter);
	auto* Observer=NewObject<USovNarrativeCueRuntimeObserver>(F.PC); F.PC->KeepAlive.Add(Observer); Observer->Tales=Tales;
	Tales->OnDialogueFinished.AddDynamic(Observer,&USovNarrativeCueRuntimeObserver::ClearDialogueDuringFinish);
	Tales->ExitDialogue(EExitDialogueReason::EDR_NoLines);
	TestTrue(TEXT("A finish listener may clear the active pointer safely"),Tales->GetCurrentDialogue()==nullptr);
	TestTrue(TEXT("The captured ending instance is still deinitialized"),Dialogue->OwningComp==nullptr);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCueControllerOutputTest, "ProjectVelkorran.Campaign.Narrative.ControllerAudioFallbackContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCueControllerOutputTest::RunTest(const FString& Parameters)
{
	auto* Audio = NewObject<UAudioComponent>(); auto* Class = NewObject<USoundClass>();
	Class->Properties.OutputTarget = EAudioOutputTarget::Controller;
	TestFalse(TEXT("Controller-only output cannot silently lose a critical cue"), FSovNarrativeCueTestAccess::ControllerOutput(Audio, Class, 1.f));
	TestNull(TEXT("Rejected routing keeps normal sound class"), Audio->SoundClassOverride.Get());
	Class->Properties.OutputTarget = EAudioOutputTarget::ControllerFallbackToSpeaker;
	TestTrue(TEXT("Approved native fallback class configured before playback"), FSovNarrativeCueTestAccess::ControllerOutput(Audio, Class, .25f));
	TestEqual(TEXT("Separate controller channel gain applied"), Audio->VolumeMultiplier, .25f);
	TestTrue(TEXT("Shared authored sound class remains the same object"), Audio->SoundClassOverride == Class);
	TestFalse(TEXT("Nonfinite channel volume rejected"), FSovNarrativeCueTestAccess::ControllerOutput(Audio, Class, std::numeric_limits<float>::quiet_NaN()));
	TestEqual(TEXT("Invalid update retains preceding gain"), Audio->VolumeMultiplier, .25f);
	TestTrue(TEXT("Optional controller channel can be muted"), FSovNarrativeCueTestAccess::ControllerOutput(Audio, Class, 0.f));
	TestEqual(TEXT("Mute applied without changing caption producer"), Audio->VolumeMultiplier, 0.f);
	return true;
}
#endif
