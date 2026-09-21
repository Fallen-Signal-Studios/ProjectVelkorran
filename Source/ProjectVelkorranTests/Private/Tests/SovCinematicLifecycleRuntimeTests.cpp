// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCinematicLifecycleRuntimeTestFixtures.h"
#include "Tests/SovNPCVisualLifecycleTestFixtures.h"
#include "GAS/AbilityConfiguration.h"
#include "Engine/EngineBaseTypes.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCinematicInventoryRuntimeTestFixtures.h"
#include "Tests/SovCompanionApproachTestFixtures.h"
#include "Components/EquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEvidenceDefinition.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovCompanionComponent.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeNPCController.h"
#include "Character/PlayerDefinition.h"
#include "Camera/PlayerCameraManager.h"
#include "Framework/SovPlayerState.h"
#include "Framework/SovApplicationLifecycleComponent.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Script.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Framework/SovPlayerController.h"
#include "Sovereign/SovGameplayTags.h"
#include "EngineUtils.h"
#include "Tests/SovTrackedContentPaths.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovCinematicInterruptionTestAccess
{
    static void Hold(USovApplicationLifecycleComponent* Component, double Started)
    { Component->State.SetReason(SovLifecyclePolicy::Reason::Background, true, Started); }
    static bool Resume(USovApplicationLifecycleComponent* Component, double Now)
    {
        Component->State.SetReason(SovLifecyclePolicy::Reason::Background, false, Now);
        return Component->State.Resume(Now);
    }
};
struct FNarrativeSequenceLifecycleTestAccess
{
	static void Play(ANarrativeLevelSequenceActor* Actor) { Actor->OnPlay(); }
	static void Stop(ANarrativeLevelSequenceActor* Actor) { Actor->OnStop(); }
	static bool Active(const ANarrativeLevelSequenceActor* Actor) { return Actor->bSessionActive; }
	static bool Pending(const ANarrativeLevelSequenceActor* Actor) { return Actor->bPendingPlayback; }
	static int32 LeaseCount(const ANarrativeLevelSequenceActor* Actor) { return Actor->OwnedParticipantTags.Num(); }
};
/** The skip hold is a timer the controller owns; nothing else about it is observable from outside. */
struct FSovCinematicSkipTestAccess
{
	static bool HoldRunning(const ASovPlayerController* PC)
	{ return PC && PC->GetWorldTimerManager().IsTimerActive(PC->SkipHoldTimer); }
	/** The exact call AcquireSystemPause and ReleaseSystemPause make; this world has no game mode. */
	static void PauseAsPauseOwnerWould(ASovPlayerController* PC, bool bPause) { PC->SetActiveCinematicPaused(bPause); }
};
struct FSovCinematicTestAccess
{
    static void StartPrepared(USovCampaignCinematicComponent* C) { C->StartPreparedPlayback(); }
    static bool HasReceipt(const USovCampaignCinematicComponent* C) { return C->bReceiptAvailable; }
    static bool ValidateParticipantState(USovCampaignCinematicComponent* C, FString& Error)
    { return C->ValidateParticipants(false, Error); }
	static bool ResolveParticipantSnapshot(USovCampaignCinematicComponent* C, FString& Error) { return C->ResolveParticipants(Error); }
	static bool ValidateSequence(USovCampaignCinematicComponent* C, ULevelSequence* Sequence, FString& Error)
	{ return C->ValidatePresentationSequence(Sequence, Error); }
	static bool AcquirePartition(USovCampaignCinematicComponent* C, FString& Error) { return C->AcquirePartitionSources(Error); }
	static bool PartitionReady(const USovCampaignCinematicComponent* C) { return C->ArePartitionRegionsReady(); }
	static void AdoptTestSource(USovCampaignCinematicComponent* C, UWorldPartitionStreamingSourceComponent* Source)
	{ FSovCinematicPartitionLease Lease; Lease.Anchor = Source->GetOwner(); Lease.Source = Source; C->PartitionLeases.Add(Lease); }
	static bool ResolveTransit(USovCampaignCinematicComponent* C, FString& Error) { return C->ResolveTransitPostconditions(Error); }
	static bool ApplyTransit(USovCampaignCinematicComponent* C, FString& Error) { return C->ApplyTransitPostconditions(Error); }
	static bool ValidateTransit(USovCampaignCinematicComponent* C, bool Applied, FString& Error) { return C->ValidateTransitPostconditions(Applied, Error); }
	static void RestoreTransit(USovCampaignCinematicComponent* C) { C->RestoreTransitPostconditions(); }
	static bool ResolveInventory(USovCampaignCinematicComponent* C, FString& Error) { return C->ResolveInventoryPostconditions(Error); }
	static bool ApplyInventory(USovCampaignCinematicComponent* C, FString& Error) { return C->ApplyInventoryPostconditions(Error); }
	static bool ValidateInventory(USovCampaignCinematicComponent* C, FString& Error) { return C->ValidateInventoryPostconditions(true, Error); }
	static void RestoreInventory(USovCampaignCinematicComponent* C) { C->RestoreParticipants(); }
	static bool InventoryNeedsRecovery(const USovCampaignCinematicComponent* C) { return C->bInventoryRollbackIncomplete; }
	static void RetireInventorySession(USovCampaignCinematicComponent* C) { C->ReleaseOwnership(); }
	static bool CommitInventoryReceipt(USovCampaignCinematicComponent* C, bool Skipped, FString& Error)
	{
		C->Phase = ESovCinematicPhase::Committing;
		C->ExpectedPlaybackGeneration = CastChecked<ANarrativeLevelSequenceActor>(C->GetOwner())->GetPlaybackGeneration();
		TGuardValue<bool> Finishing(C->bFinishing, true); return C->CommitNativePostconditions(Skipped, Error);
	}
	static void ExpireLoading(USovCampaignCinematicComponent* C)
    { C->LoadingStartedSeconds = C->PreparationTimeSeconds() - C->LoadingTimeoutSeconds - 1.; C->TickComponent(0.f, LEVELTICK_PauseTick, nullptr); }
	static bool CheckWatchdog(USovCampaignCinematicComponent* C) { return C->CheckPreparationWatchdog(C->RequestEpoch); }
    static void StageLoadingAge(USovCampaignCinematicComponent* C, double Age)
    { C->LoadingStartedSeconds = C->PreparationTimeSeconds() - Age; }
	static void StageOwnedSession(USovCampaignCinematicComponent* C, ASovHandoffRuntimeTestController* PC,
		ASovHandoffRuntimeTestPawn* Pawn, UAbilitySystemComponent* ASC)
	{
		C->Controller = PC; C->PlayerPawn = Pawn; C->PlayerASC = ASC;
		C->OriginalViewTarget = PC->GetViewTarget(); C->OriginalControlRotation = PC->GetControlRotation();
		FSovCinematicParticipantSnapshot Entry; Entry.Character = Pawn; Entry.ASC = ASC;
		Entry.Transform = Pawn->GetActorTransform(); Entry.Wield = Pawn->GetWeaponWieldState();
		Entry.OriginalWieldRevision = Pawn->GetWeaponWieldRevision();
		C->InventoryChanges.Reset(); C->EquipmentSnapshots.Reset(); C->bInventoryPostconditionsApplied = false;
		C->bInventoryTransactionCommitted = false; C->bInventoryRollbackIncomplete = false;
		C->Snapshot = {Entry}; C->Phase = ESovCinematicPhase::Loading; ++C->RequestEpoch;
		C->ReservedPlaybackGeneration = CastChecked<ANarrativeLevelSequenceActor>(C->GetOwner())->GetPlaybackGeneration();
		C->SessionId = FGuid::NewGuid(); C->bOwnInput = true;
		C->LoadingStartedSeconds = C->PreparationTimeSeconds();
		PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
		C->bOwnSequenceTag = true; ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	}
	static void FinishSupersededSession(USovCampaignCinematicComponent* C, uint64 CurrentGeneration)
	{
		C->Phase = ESovCinematicPhase::Playing; C->ExpectedPlaybackGeneration = CurrentGeneration - 1;
		C->PlayedSeconds = 100.0; C->DurationSeconds = .01; C->bFullViewEligible = true;
		C->HandleFinished();
	}
	static void StagePlaying(USovCampaignCinematicComponent* C, uint64 Generation)
	{ C->ExpectedPlaybackGeneration = Generation; C->ChangePhase(ESovCinematicPhase::Playing); }
	static void RestartDuringCommit(USovCampaignCinematicComponent* C, ANarrativeLevelSequenceActor* Actor)
	{
		C->ExpectedPlaybackGeneration = Actor->GetPlaybackGeneration(); C->Phase = ESovCinematicPhase::Committing;
		C->bFinishing = true;
		Actor->GetSequencePlayer()->OnPlay.AddUniqueDynamic(C, &USovCampaignCinematicComponent::HandleStarted);
		Actor->GetSequencePlayer()->Play();
		C->bFinishing = false;
	}
	static bool OwnsAnything(const USovCampaignCinematicComponent* C) { return C->bOwnInput || C->bOwnSequenceTag || C->bReceiptAvailable || !C->PartitionLeases.IsEmpty(); }
	static void LeaveWorld(USovCampaignCinematicComponent* C) { C->EndPlay(EEndPlayReason::Destroyed); }
};
namespace
{
	struct FSequenceWorld
	{
		// Transient fixture worlds still need actor delegates dispatched through ProcessEvent.
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovSequenceLifecycleTestActor* Actor = nullptr;
		ASovAxiomRuntimeTestCharacter* Participant = nullptr;
		ULevelSequence* Sequence = nullptr;
		UNarrativeAbilitySystemComponent* ASC = nullptr;
		USovSequenceLifecycleProbe* Probe = nullptr;
		FSequenceWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			Actor = World->SpawnActor<ASovSequenceLifecycleTestActor>();
			Participant = World->SpawnActor<ASovAxiomRuntimeTestCharacter>();
			if (!Actor || !Participant) { return; }
			Participant->InitializeTestCombat(0); ASC = Participant->GetNarrativeAbilitySystemComponent();
			Sequence = NewObject<ULevelSequence>(Actor); Sequence->Initialize();
			Sequence->GetMovieScene()->SetPlaybackRange(0, 240000);
			Actor->InitializeTestSequence(Sequence);
			FNarrativeSequencePlaybackSettings Settings; Settings.bAutoPlay = false;
			Actor->UpdateSequence(Sequence, Settings);
			Actor->TestParticipants.Add(Participant); Actor->TestParticipants.Add(Participant);
			Probe = NewObject<USovSequenceLifecycleProbe>(Actor);
			Actor->OnPlaybackFailed.AddDynamic(Probe, &USovSequenceLifecycleProbe::Failed);
			Actor->OnBlendOutFinished.AddDynamic(Probe, &USovSequenceLifecycleProbe::Blended);
		}
		~FSequenceWorld()
		{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	};
	struct FManagedSequenceWorld
	{
		FSequenceWorld Base;
		ASovHandoffRuntimeTestController* PC = nullptr;
		ASovHandoffRuntimeTestPawn* Pawn = nullptr;
		UNarrativeAbilitySystemComponent* ASC = nullptr;
		USovCampaignCinematicComponent* Component = nullptr;
		ASovNPCVisualLifecycleTestVisual* PendingVisual = nullptr;
		bool PreparePendingVisual()
		{
			if (!Component || !ASC || !static_cast<ANarrativeCharacter*>(Pawn)->GetCharacterDefinition()) { return false; }
			// Keep gameplay readiness intact while the real appearance producer is pending,
			// matching the observed Selene startup window. Do not override ParticipantsReady.
			static_cast<ANarrativeCharacter*>(Pawn)->GetCharacterDefinition()->AbilityConfiguration = NewObject<UAbilityConfiguration>(Pawn);
			Pawn->ApplyCinematicStartupEffectsForTest();
			FActorSpawnParameters Params; Params.Owner = Pawn;
			PendingVisual = Base.World->SpawnActor<ASovNPCVisualLifecycleTestVisual>(Params);
			if (!PendingVisual) { return false; }
			PendingVisual->SetCharacterForTest(Pawn); Pawn->SetCinematicVisualForTest(PendingVisual);
			Base.Actor->bUseNativeBindings = true;
			auto* Scene = Base.Sequence->GetMovieScene();
			const FGuid Binding = Scene->AddPossessable(TEXT("Player"), ANarrativeCharacter::StaticClass());
			Scene->TagBinding(TEXT("Player"), UE::MovieScene::FFixedObjectBindingID(Binding, MovieSceneSequenceID::Root));
			Base.Actor->GetSequencePlayer()->OnPlay.AddDynamic(Base.Probe, &USovSequenceLifecycleProbe::Started);
			FSovCinematicTestAccess::StageOwnedSession(Component, PC, Pawn, ASC);
			return Pawn->IsCharacterReady() && Pawn->IsCharacterPendingLoad() && ASC->bStartupEffectsApplied;
		}
		FManagedSequenceWorld(bool bIncludeReplayBeat = false, bool bSharedObserverMission = false)
		{
			if (!Base.World || !Base.Actor) { return; }
			PC = Base.World->SpawnActor<ASovHandoffRuntimeTestController>();
			Pawn = Base.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
			auto* PS = Base.World->SpawnActor<ASovPlayerState>();
			if (!PC || !Pawn || !PS) { return; }
			auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
			Pawn->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Pawn);
			if (!Pawn->StageTestReadiness(PS, true) || !Pawn->CompleteCampaignDataInitialization(false)) { return; }
			ASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
			// Transient worlds do not run the controller initialization that owns the camera.
			if (!PC->PlayerCameraManager) { PC->PlayerCameraManager = Base.World->SpawnActor<APlayerCameraManager>(); }
			if (!PC->PlayerCameraManager) { return; }
			PC->PlayerCameraManager->InitializeFor(PC);
			PC->SetViewTarget(Pawn);
			PC->PlayerCameraManager->UpdateCamera(0.f);
			auto* Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
			Mission->MissionId = bSharedObserverMission ? FName(TEXT("M13_SharedWitnessTest")) : FName(TEXT("CinematicRuntime"));
			Mission->Protagonist = Pawn->GetProtagonistIdentityTag();
			Mission->PawnClass = Pawn->GetClass(); Mission->PlayerDefinition = Definition;
			FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("Scene"); Beat.CinematicId = TEXT("FirstView"); Beat.bRequiresCinematicProof = true;
			Mission->Beats.Add(Beat);
			if (bIncludeReplayBeat)
			{ FSovCampaignBeatDefinition Replay = Beat; Replay.BeatId = TEXT("Replay"); Replay.PrerequisiteBeats = {Beat.BeatId}; Mission->Beats.Add(Replay); }
			if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { return; }
			Component = NewObject<USovCampaignCinematicComponent>(Base.Actor); Base.Actor->AddInstanceComponent(Component); Component->RegisterComponent();
			// Match the tick prerequisite normally established when the world begins play.
			Component->RegisterAllComponentTickFunctions(true); Component->SetComponentTickEnabled(true);
			Component->MissionId = Mission->MissionId; Component->BeatId = Beat.BeatId; Component->Sequence = Base.Sequence;
			FSovCinematicParticipant Participant; Participant.BindingTag = TEXT("Player"); Participant.bControlledProtagonist = true;
			Component->Participants.Add(Participant);
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicOwnershipTest, "ProjectVelkorran.Campaign.Cinematic.ExactOwnershipAndResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicOwnershipTest::RunTest(const FString& Parameters)
{
	FSequenceWorld F; if (!TestNotNull(TEXT("Native cinematic test ASC"), F.ASC)) { return false; }
	const FGameplayTag Protected = FNarrativeGameplayTags::Get().State_Invulnerable;
	const FGameplayTag Controlled = FNarrativeGameplayTags::Get().State_SequencerControlled;
	F.ASC->AddLooseGameplayTag(Protected);
	FNarrativeSequenceLifecycleTestAccess::Play(F.Actor);
	TestEqual(TEXT("Duplicate bound objects acquire one ASC lease"), FNarrativeSequenceLifecycleTestAccess::LeaseCount(F.Actor), 1);
	TestEqual(TEXT("Sequence adds exactly one protection count"), F.ASC->GetGameplayTagCount(Protected), 2);
	FNarrativeSequenceLifecycleTestAccess::Play(F.Actor);
	TestEqual(TEXT("Resume cannot stack the same tag ownership"), F.ASC->GetGameplayTagCount(Protected), 2);
	TestEqual(TEXT("Resume cannot stack controller state ownership"), F.ASC->GetGameplayTagCount(Controlled), 1);
	FNarrativeSequenceLifecycleTestAccess::Stop(F.Actor);
	TestEqual(TEXT("Stop preserves unrelated protection"), F.ASC->GetGameplayTagCount(Protected), 1);
	TestFalse(TEXT("Stop removes this sequence's control tag"), F.ASC->HasMatchingGameplayTag(Controlled));
	FNarrativeSequenceLifecycleTestAccess::Stop(F.Actor);
	TestEqual(TEXT("Repeated terminal callbacks do not over-release another owner"), F.ASC->GetGameplayTagCount(Protected), 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicReentryTest, "ProjectVelkorran.Campaign.Cinematic.ReentrantTagStopAndDirectPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicReentryTest::RunTest(const FString& Parameters)
{
	FSequenceWorld F; if (!TestNotNull(TEXT("Native cinematic test ASC"), F.ASC)) { return false; }
	const auto& Tags = FNarrativeGameplayTags::Get();
	F.ASC->AddLooseGameplayTag(Tags.State_Invulnerable);
	bool bStopOnce = true;
	const FDelegateHandle StopHandle = F.ASC->RegisterGameplayTagEvent(Tags.State_SequencerControlled, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([&](FGameplayTag Tag, int32 Count)
		{ if (Count > 0 && bStopOnce) { bStopOnce = false; FNarrativeSequenceLifecycleTestAccess::Stop(F.Actor); } });
	FNarrativeSequenceLifecycleTestAccess::Play(F.Actor);
	TestFalse(TEXT("Tag callback can retire the session before controller notifications"), FNarrativeSequenceLifecycleTestAccess::Active(F.Actor));
	TestEqual(TEXT("Reentrant stop leaves no participant leases"), FNarrativeSequenceLifecycleTestAccess::LeaseCount(F.Actor), 0);
	TestEqual(TEXT("Unrelated protection remains after partial tag acquisition"), F.ASC->GetGameplayTagCount(Tags.State_Invulnerable), 1);
	TestFalse(TEXT("No late acquisition leaks after stop"), F.ASC->HasMatchingGameplayTag(Tags.State_SequencerControlled));
	F.ASC->RegisterGameplayTagEvent(Tags.State_SequencerControlled, EGameplayTagEventType::NewOrRemoved).Remove(StopHandle);
	FNarrativeSequenceLifecycleTestAccess::Play(F.Actor);
	bool bPlayOnce = true;
	const FDelegateHandle PlayHandle = F.ASC->RegisterGameplayTagEvent(Tags.State_SequencerControlled, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([&](FGameplayTag Tag, int32 Count)
		{ if (Count == 0 && bPlayOnce) { bPlayOnce = false; F.Actor->GetSequencePlayer()->Play(); } });
	FNarrativeSequenceLifecycleTestAccess::Stop(F.Actor);
	TestFalse(TEXT("Direct engine Play from teardown cannot leave unowned playback running"), F.Actor->GetSequencePlayer()->IsPlaying());
	TestFalse(TEXT("Direct Play during teardown cannot reacquire the retired session"), FNarrativeSequenceLifecycleTestAccess::Active(F.Actor));
	F.ASC->RegisterGameplayTagEvent(Tags.State_SequencerControlled, EGameplayTagEventType::NewOrRemoved).Remove(PlayHandle);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicTimeoutAndGenerationTest, "ProjectVelkorran.Campaign.Cinematic.TimeoutAndRequestIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicTimeoutAndGenerationTest::RunTest(const FString& Parameters)
{
	FSequenceWorld F; if (!TestNotNull(TEXT("Native cinematic test actor"), F.Actor)) { return false; }
	FNarrativeSequencePlaybackSettings Missing; Missing.bAutoPlay = true; Missing.RequiredParticipantBindingTags.Add(TEXT("RequiredMissingActor"));
	Missing.ParticipantReadyTimeoutSeconds = .1f;
	F.Actor->UpdateSequence(F.Sequence, Missing);
	TestTrue(TEXT("Missing required actor enters bounded pending state"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Actor));
	F.Actor->Tick(.11f);
	TestFalse(TEXT("Readiness timeout clears pending state"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Actor));
	TestEqual(TEXT("Readiness timeout reports failure once"), F.Probe->FailedCount, 1);
	F.Actor->Tick(.11f); TestEqual(TEXT("Timeout cannot refire every tick"), F.Probe->FailedCount, 1);
	FNarrativeSequencePlaybackSettings Manual; Manual.bAutoPlay = false;
	auto* Action = UAsyncAction_PlayNarrativeSequence::PlayNarrativeSequence(F.Actor, F.Sequence, Manual);
	if (!TestNotNull(TEXT("Existing async node created"), Action)) { return false; }
	Action->OnFinished.AddDynamic(F.Probe, &USovSequenceLifecycleProbe::Finished);
	Action->OnInterrupted.AddDynamic(F.Probe, &USovSequenceLifecycleProbe::Interrupted);
	Action->Activate();
	F.Actor->UpdateSequence(F.Sequence, Manual); // Another request supersedes the same shared engine player.
	F.Actor->GetSequencePlayer()->OnFinished.Broadcast();
	TestEqual(TEXT("New request completion cannot complete the old async action"), F.Probe->FinishedCount, 0);
	TestEqual(TEXT("Old action reports interruption once"), F.Probe->InterruptedCount, 1);
	F.Actor->GetSequencePlayer()->OnFinished.Broadcast();
	TestEqual(TEXT("Repeated completion cannot refire a retired action"), F.Probe->InterruptedCount, 1);
	F.Actor->BlendOutSeconds = 0.f; F.Actor->BlendOutAndStop();
	TestEqual(TEXT("Explicit zero-duration blend has a finite terminal notification"), F.Probe->BlendCount, 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignCinematicValidationTest, "ProjectVelkorran.Campaign.Cinematic.ManifestAndPresentationValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignCinematicValidationTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	FString Error;
	TestTrue(TEXT("One controlled protagonist and finite presentation is valid"), F.Component->ValidateConfiguration(Error));
	const FSovCinematicParticipant DuplicateParticipant = F.Component->Participants[0];
	F.Component->Participants.Add(DuplicateParticipant);
	TestFalse(TEXT("Duplicate participant binding is rejected"), F.Component->ValidateConfiguration(Error));
	F.Component->Participants.SetNum(1);
	F.Component->Participants[0].ExitWield = ESovCinematicExitWield::DrawRequiredWeapon;
	TestFalse(TEXT("Draw cannot invent an unowned unconfigured weapon"), F.Component->ValidateConfiguration(Error));
	F.Component->Participants[0].ExitWield = ESovCinematicExitWield::Keep;
	F.Component->PreloadAssets.SetNum(129);
	TestFalse(TEXT("Unbounded preload manifest is rejected"), F.Component->ValidateConfiguration(Error));
	F.Component->PreloadAssets.Reset();
	TestTrue(TEXT("Finite presentation-only sequence is accepted"), FSovCinematicTestAccess::ValidateSequence(F.Component, F.Base.Sequence, Error));
	F.Base.Sequence->GetMovieScene()->AddTrack<UMovieSceneEventTrack>();
	TestFalse(TEXT("Arbitrary event tracks cannot become managed campaign postconditions"), FSovCinematicTestAccess::ValidateSequence(F.Component, F.Base.Sequence, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignCinematicProofTest, "ProjectVelkorran.Campaign.Cinematic.NativeProofAndSupersededFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignCinematicProofTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	auto* State = F.PC->GetCampaignState();
	TestEqual(TEXT("Generic beat completion cannot bypass the native presentation"), State->CompleteBeat(TEXT("Scene")), ESovCampaignResult::Invalid);
	TestFalse(TEXT("Legacy viewed setter cannot mint a managed scene's first-view proof"), State->RecordCinematicViewed(TEXT("Scene")));
	TestFalse(TEXT("First viewing cannot be skipped"), State->CanSkipCinematic(TEXT("Scene")));
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	FNarrativeSequencePlaybackSettings Manual; Manual.bAutoPlay = false;
	F.Base.Actor->UpdateSequence(F.Base.Sequence, Manual); F.Base.Actor->GetSequencePlayer()->Play();
	const FVector NewOwnerPosition(300, 0, 100); F.Pawn->SetActorLocation(NewOwnerPosition); F.PC->SetViewTarget(F.Base.Actor);
	FSovCinematicTestAccess::FinishSupersededSession(F.Component, F.Base.Actor->GetPlaybackGeneration());
	TestTrue(TEXT("Retiring a superseded request cannot stop the newer engine playback"), F.Base.Actor->GetSequencePlayer()->IsPlaying());
	TestTrue(TEXT("Old rollback cannot overwrite the newer participant placement"), F.Pawn->GetActorLocation().Equals(NewOwnerPosition));
	TestTrue(TEXT("Old camera restoration cannot overwrite the newer view target"), F.PC->GetViewTarget() == F.Base.Actor);
	TestFalse(TEXT("Another playback generation cannot commit this scene"), State->IsBeatComplete(F.Component->MissionId, TEXT("Scene")));
	TestEqual(TEXT("Superseded finish terminates the invalid request"), F.Component->GetPhase(), ESovCinematicPhase::Failed);
	TestFalse(TEXT("Superseded request leaves no component leases"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignCinematicAbortTest, "ProjectVelkorran.Campaign.Cinematic.AbortOwnershipAndEndPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignCinematicAbortTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	F.Component->RegisterAllComponentTickFunctions(true);
	F.Component->BeginPlay();
	const FGameplayTag Controlled = FNarrativeGameplayTags::Get().State_SequencerControlled;
	F.PC->SetIgnoreMoveInput(true); F.PC->SetIgnoreLookInput(true); F.ASC->AddLooseGameplayTag(Controlled);
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	F.Component->Abort(TEXT("Test interrupted loading"));
	TestEqual(TEXT("Abort removes only the component's sequence tag contribution"), F.ASC->GetGameplayTagCount(Controlled), 1);
	TestTrue(TEXT("Abort preserves unrelated movement suppression"), F.PC->IsMoveInputIgnored());
	TestTrue(TEXT("Abort preserves unrelated look suppression"), F.PC->IsLookInputIgnored());
	TestFalse(TEXT("Abort retires all component ownership"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	F.Component->Abort(TEXT("Late duplicate"));
	TestEqual(TEXT("Duplicate abort does not over-release another owner"), F.ASC->GetGameplayTagCount(Controlled), 1);
	F.PC->SetIgnoreMoveInput(false); F.PC->SetIgnoreLookInput(false); F.ASC->RemoveLooseGameplayTag(Controlled);
	TestFalse(TEXT("No hidden movement lease remains after the external owner releases"), F.PC->IsMoveInputIgnored());
	TestFalse(TEXT("No hidden look lease remains after the external owner releases"), F.PC->IsLookInputIgnored());
	FSovCinematicTestAccess::LeaveWorld(F.Component); FString Error;
	TestFalse(TEXT("An ended component cannot accept another request"), F.Component->RequestPlay(F.PC, Error));
	TestFalse(TEXT("Aborting presentation never commits its campaign beat"), F.PC->GetCampaignState()->IsBeatComplete(F.Component->MissionId, TEXT("Scene")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignCinematicPauseReentryTest, "ProjectVelkorran.Campaign.Cinematic.PauseCallbackCannotResurrectAbort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignCinematicPauseReentryTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	F.Base.Actor->GetSequencePlayer()->Play();
	FSovCinematicTestAccess::StagePlaying(F.Component, F.Base.Actor->GetPlaybackGeneration());
	F.Base.Probe->Managed = F.Component;
	F.Base.Actor->GetSequencePlayer()->OnPause.AddDynamic(F.Base.Probe, &USovSequenceLifecycleProbe::AbortManaged);
	// UE 5.7 delivers OnPause after queued evaluation completes, as the game tick would do.
	TestTrue(TEXT("Pause request is accepted before its deferred callback runs"), F.Component->SetCinematicPaused(true));
	TestTrue(TEXT("Accepted request pauses the real engine player"), F.Base.Actor->GetSequencePlayer()->IsPaused());
	auto Runner = F.Base.Actor->GetSequencePlayer()->GetEvaluationTemplate().GetRunner();
	if (!TestTrue(TEXT("Real evaluation runner owns the deferred pause callback"), Runner.IsValid())) { return false; }
	Runner->Flush();
	TestEqual(TEXT("Outer pause cannot resurrect a retired phase"), F.Component->GetPhase(), ESovCinematicPhase::Failed);
	TestFalse(TEXT("Reentrant pause interruption releases component ownership"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	TestFalse(TEXT("Matching interrupted sequence is stopped"), F.Base.Actor->GetSequencePlayer()->IsPlaying());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignCinematicDirectRestartTest, "ProjectVelkorran.Campaign.Cinematic.DirectRestartDuringCommitRetiresOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignCinematicDirectRestartTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	FSovCinematicTestAccess::RestartDuringCommit(F.Component, F.Base.Actor);
	const FVector NewPosition(400, 0, 100); F.Pawn->SetActorLocation(NewPosition); F.PC->SetViewTarget(F.Base.Actor);
	F.Component->Abort(TEXT("Retire interrupted commit"));
	TestTrue(TEXT("Direct restart during commit cannot be stopped by retired cleanup"), F.Base.Actor->GetSequencePlayer()->IsPlaying());
	TestTrue(TEXT("Retired cleanup cannot restore old placement over the restarted scene"), F.Pawn->GetActorLocation().Equals(NewPosition));
	TestTrue(TEXT("Retired cleanup cannot restore old camera over the restarted scene"), F.PC->GetViewTarget() == F.Base.Actor);
	TestFalse(TEXT("Retired component releases its own leases"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	TestFalse(TEXT("Direct restart cannot supply first-view proof"), F.PC->GetCampaignState()->IsBeatComplete(F.Component->MissionId, TEXT("Scene")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicPartitionManifestTest, "ProjectVelkorran.Campaign.Cinematic.PartitionManifestAndClassicWorldRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicPartitionManifestTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	FString Error;
	TestTrue(TEXT("Classic scenes do not require World Partition"), FSovCinematicTestAccess::PartitionReady(F.Component));
	FSovCinematicPartitionRegion Region; Region.RegionId = TEXT("CameraDestination"); Region.RequiredActors.Add(F.Base.Participant);
	F.Component->RequiredPartitionRegions.Add(Region);
	TestTrue(TEXT("Bounded partition declaration validates before runtime world gate"), F.Component->ValidateConfiguration(Error));
	TestFalse(TEXT("Partition declaration fails explicitly in a conventional world"), FSovCinematicTestAccess::AcquirePartition(F.Component, Error));
	TestFalse(TEXT("An unacquired partition manifest cannot count as ready"), FSovCinematicTestAccess::PartitionReady(F.Component));
	F.Component->RequiredPartitionRegions[0].RequiredActors.Reset();
	TestFalse(TEXT("No activation witness cannot prove a cell was streamed"), F.Component->ValidateConfiguration(Error));
	F.Component->RequiredPartitionRegions[0] = Region; F.Component->RequiredPartitionRegions.Add(Region);
	TestFalse(TEXT("Duplicate region identities are rejected"), F.Component->ValidateConfiguration(Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicPartitionCleanupTest, "ProjectVelkorran.Campaign.Cinematic.PartitionOwnedTeardownAndPausedTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicPartitionCleanupTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	const auto MakeSource = [&F]()
	{
		AActor* Anchor = F.Base.World->SpawnActor<AActor>();
		auto* Source = NewObject<UWorldPartitionStreamingSourceComponent>(Anchor);
		Anchor->AddInstanceComponent(Source); Source->RegisterComponent(); Source->EnableStreamingSource(); return Source;
	};
	auto* Other = MakeSource(); auto* Owned = MakeSource(); TWeakObjectPtr<AActor> OwnedAnchor = Owned->GetOwner();
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	FSovCinematicTestAccess::AdoptTestSource(F.Component, Owned);
	FSovCinematicTestAccess::ExpireLoading(F.Component);
	TestEqual(TEXT("Preparation wall-clock timeout runs at zero game delta"), F.Component->GetPhase(), ESovCinematicPhase::Failed);
	TestFalse(TEXT("Timeout retires owned source registration"), IsValid(Owned) && Owned->IsRegistered());
	TestTrue(TEXT("Timeout destroys only the owned anchor"), !OwnedAnchor.IsValid() || OwnedAnchor->IsActorBeingDestroyed());
	TestTrue(TEXT("Unrelated streaming source remains registered and enabled"), IsValid(Other) && Other->IsRegistered() && Other->IsStreamingSourceEnabled());
	TestFalse(TEXT("Timeout releases component input and tag leases"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	Owned = MakeSource(); FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	FSovCinematicTestAccess::AdoptTestSource(F.Component, Owned); F.Component->UnregisterComponent();
	TestFalse(TEXT("Unregister removes a source even before normal EndPlay"), IsValid(Owned) && Owned->IsRegistered());
	TestTrue(TEXT("Unregister preserves a source owned elsewhere"), Other->IsRegistered() && Other->IsStreamingSourceEnabled());
	FString Error;
	TestFalse(TEXT("A retained unregistered component cannot reacquire cinematic ownership"), F.Component->RequestPlay(F.PC, Error));
	TestFalse(TEXT("Rejected unregistered request leaves no leases"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	F.Component->RegisterComponent(); F.Component->SetComponentTickEnabled(true);
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	Owned = MakeSource(); FSovCinematicTestAccess::AdoptTestSource(F.Component, Owned);
	F.Component->SetComponentTickEnabled(false);
	TestFalse(TEXT("Independent core watchdog retires accidentally disabled component ticking"), FSovCinematicTestAccess::CheckWatchdog(F.Component));
	TestFalse(TEXT("Tick disable cannot strand source/input/tag leases"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicConsoleInterruptionTest, "ProjectVelkorran.Campaign.Cinematic.ConsoleInterruptionPreservesPreparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicConsoleInterruptionTest::RunTest(const FString& Parameters)
{
    FManagedSequenceWorld F;
    if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
    auto* Lifecycle = F.PC->GetApplicationLifecycle();
    if (!TestNotNull(TEXT("Controller's production lifecycle owner"), Lifecycle)) { return false; }
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    // Simulate a suspend longer than the complete load budget without sleeping or
    // sending global platform delegates to the editor running this test.
    FSovCinematicInterruptionTestAccess::Hold(Lifecycle, FPlatformTime::Seconds() - 120.);
    FSovCinematicTestAccess::StageLoadingAge(F.Component, 2.);
    TestTrue(TEXT("Independent watchdog remains alive during interruption"), FSovCinematicTestAccess::CheckWatchdog(F.Component));
    TestEqual(TEXT("Suspended load keeps its preparation phase"), F.Component->GetPhase(), ESovCinematicPhase::Loading);
    FString Error;
    TestFalse(TEXT("Interrupted gameplay cannot launch another cinematic transaction"), F.Component->RequestPlay(F.PC, Error));
    TestTrue(TEXT("Only explicit resume retires the held interval"), FSovCinematicInterruptionTestAccess::Resume(Lifecycle, FPlatformTime::Seconds()));
    TestTrue(TEXT("Two minutes of suspend do not exhaust two seconds of active loading"), FSovCinematicTestAccess::CheckWatchdog(F.Component));
    FSovCinematicTestAccess::ExpireLoading(F.Component);
    TestEqual(TEXT("Genuine active-time timeout still fails and releases ownership"), F.Component->GetPhase(), ESovCinematicPhase::Failed);
    TestFalse(TEXT("Timeout does not strand cinematic leases"), FSovCinematicTestAccess::OwnsAnything(F.Component));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicTransitContractTest, "ProjectVelkorran.Campaign.Cinematic.NativeTransitPostconditionAndRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicTransitContractTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	auto* Transit = F.Base.World->SpawnActor<ASovWorldTransitActor>();
	if (!TestNotNull(TEXT("Native transit owner"), Transit)) { return false; }
	Transit->TransitId = TEXT("ExitDoor"); Transit->SetPower(false); Transit->SetLockReason(FText::FromString(TEXT("Before scene")));
	FSovCinematicTransitPostcondition Contract; Contract.Transit = Transit; Contract.ExpectedTransitId = Transit->TransitId;
	Contract.bSetPower = true; Contract.bPowered = true; Contract.bSetLock = true;
	F.Component->TransitPostconditions.Add(Contract); FString Error;
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	TestTrue(TEXT("Native stable mechanism snapshots successfully"), FSovCinematicTestAccess::ResolveTransit(F.Component, Error));
	TestTrue(TEXT("The completion/skip shared postcondition applies through native setters"), FSovCinematicTestAccess::ApplyTransit(F.Component, Error));
	TestTrue(TEXT("Power and lock postconditions are actually present"), Transit->bPowered && Transit->LockReason.IsEmpty());
	TestTrue(TEXT("Final receipt validation observes the applied world state"), FSovCinematicTestAccess::ValidateTransit(F.Component, true, Error));
	FSovCinematicTestAccess::RestoreTransit(F.Component);
	TestTrue(TEXT("Failed commit restores its native power and lock values"), !Transit->bPowered && Transit->LockReason.ToString() == TEXT("Before scene"));
	TestFalse(TEXT("Applying native world state alone cannot mint a campaign beat"), F.PC->GetCampaignState()->IsBeatComplete(F.Component->MissionId, TEXT("Scene")));
	Transit->StructuralHealth = 0.f;
	TestFalse(TEXT("Destroyed mechanism cannot be repaired by cinematic power writes"), FSovCinematicTestAccess::ResolveTransit(F.Component, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicTransitReentryTest, "ProjectVelkorran.Campaign.Cinematic.TransitCallbackConflictPreservesExternalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicTransitReentryTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	auto* Transit = F.Base.World->SpawnActor<ASovWorldTransitActor>();
	if (!TestNotNull(TEXT("Native transit owner"), Transit)) { return false; }
	Transit->TransitId = TEXT("ExitDoor"); Transit->SetPower(false);
	FSovCinematicTransitPostcondition Contract; Contract.Transit = Transit; Contract.ExpectedTransitId = Transit->TransitId;
	Contract.bSetPower = true; Contract.bPowered = true; Contract.bSetLock = true;
	F.Component->TransitPostconditions.Add(Contract); FString Error;
	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	TestTrue(TEXT("Native transit snapshot"), FSovCinematicTestAccess::ResolveTransit(F.Component, Error));
	F.Base.Probe->Transit = Transit;
	Transit->OnTransitChanged.AddDynamic(F.Base.Probe, &USovSequenceLifecycleProbe::ChangeTransitLock);
	TestFalse(TEXT("A conflicting native callback aborts the postcondition"), FSovCinematicTestAccess::ApplyTransit(F.Component, Error));
	FSovCinematicTestAccess::RestoreTransit(F.Component);
	TestFalse(TEXT("Our power change is restored"), Transit->bPowered);
	TestEqual(TEXT("External lock owner is preserved by rollback"), Transit->LockReason.ToString(), FString(TEXT("External lock owner")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicTransitRevisionTest, "ProjectVelkorran.Campaign.Cinematic.TransitRollbackSameValueOwnershipAndDestruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicTransitRevisionTest::RunTest(const FString& Parameters)
{
	for (const bool bDamageDuringRestore : {false, true})
	{
		FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
		auto* Transit = F.Base.World->SpawnActor<ASovWorldTransitActor>();
		if (!TestNotNull(TEXT("Native transit owner"), Transit)) { return false; }
		Transit->TransitId = TEXT("ExitDoor"); Transit->SetPower(false); Transit->SetLockReason(FText::FromString(TEXT("Before scene")));
		FSovCinematicTransitPostcondition Contract; Contract.Transit = Transit; Contract.ExpectedTransitId = Transit->TransitId;
		Contract.bSetPower = true; Contract.bPowered = true; Contract.bSetLock = true;
		F.Component->TransitPostconditions.Add(Contract); FString Error;
		FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
		TestTrue(TEXT("Snapshot native transit revision"), FSovCinematicTestAccess::ResolveTransit(F.Component, Error));
		TestTrue(TEXT("Apply exact native setter revisions"), FSovCinematicTestAccess::ApplyTransit(F.Component, Error));
		if (bDamageDuringRestore)
		{
			F.Base.Probe->Transit = Transit;
			Transit->OnTransitChanged.AddDynamic(F.Base.Probe, &USovSequenceLifecycleProbe::DamageTransitOnChange);
		}
		else
		{
			Transit->SetPower(true); Transit->SetLockReason(FText());
			TestFalse(TEXT("A later same-value native write invalidates the original receipt"), FSovCinematicTestAccess::ValidateTransit(F.Component, true, Error));
		}
		FSovCinematicTestAccess::RestoreTransit(F.Component);
		if (bDamageDuringRestore)
		{
			TestEqual(TEXT("Power rollback callback can break the mechanism"), Transit->GetTransitState(), ESovWorldTransitState::Broken);
			TestTrue(TEXT("No subsequent lock rollback touches the broken mechanism"), Transit->LockReason.IsEmpty());
		}
		else
		{
			TestTrue(TEXT("Later same-value power owner survives rollback"), Transit->bPowered);
			TestTrue(TEXT("Later same-value lock owner survives rollback"), Transit->LockReason.IsEmpty());
		}
	}
	return true;
}
namespace
{
    void AddCinematicReplacementManifest(FManagedSequenceWorld& F, UEquippableItem* Previous)
    {
        F.Component->Participants[0].ExitWield = ESovCinematicExitWield::Holster;
        FSovCinematicInventoryPostcondition Remove; Remove.ParticipantBinding = TEXT("Player");
        Remove.Mutation.MutationId = TEXT("TakeOldEquipment"); Remove.Mutation.Operation = ENarrativeCinematicItemOperation::Remove;
        Remove.Mutation.ItemClass = Previous->GetClass(); Remove.Mutation.ItemGUID = Previous->ItemGUID;
        FSovCinematicInventoryPostcondition Grant; Grant.ParticipantBinding = TEXT("Player");
        Grant.Mutation.MutationId = TEXT("GiveReplacement"); Grant.Mutation.ItemClass = USovCinematicTestEquipmentReplacement::StaticClass();
        F.Component->InventoryPostconditions = {Remove, Grant};
        FSovCinematicEquipmentPostcondition Equipment; Equipment.ParticipantBinding = TEXT("Player");
        Equipment.EquipmentSlot = FNarrativeGameplayTags::Get().Equipment_Slot_Ammo;
        Equipment.PreviousItemClass = Previous->GetClass(); Equipment.PreviousItemGUID = Previous->ItemGUID;
        Equipment.ReplacementItemClass = USovCinematicTestEquipmentReplacement::StaticClass(); Equipment.ReplacementGrantId = Grant.Mutation.MutationId;
        F.Component->EquipmentPostconditions = {Equipment};
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicEquipmentReplacementTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.FullCapacityEquipmentReplacementAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicEquipmentReplacementTest::RunTest(const FString& Parameters)
{
    FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic"), F.Component)) { return false; }
    auto* Inventory = F.Pawn->GetInventoryComponent();
    auto Added = Inventory->TryAddItemFromClass(USovCinematicTestEquipment::StaticClass(), 1, false);
    if (!TestEqual(TEXT("Original equipment exists"), Added.AmountGiven, 1)) { return false; }
    auto* Original = CastChecked<UEquippableItem>(Added.Stacks[0]); const FGuid OriginalGUID = Original->ItemGUID;
    const auto Slot = FNarrativeGameplayTags::Get().Equipment_Slot_Ammo;
    if (!Original->IsEquipped()) { TestTrue(TEXT("Equip original"), Inventory->SetCinematicEquipment(Original, Slot)); }
    Original->SetLastUseTime(23.f); Inventory->SetCapacity(Inventory->GetItems().Num());
    AddCinematicReplacementManifest(F, Original); FString Error;
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    TestTrue(TEXT("Resolve approved exact inventory/equipment"), FSovCinematicTestAccess::ResolveInventory(F.Component, Error));
    TestTrue(TEXT("Remove-before-grant replacement succeeds at full capacity"), FSovCinematicTestAccess::ApplyInventory(F.Component, Error));
    auto* Replacement = F.Pawn->GetEquipmentComponent()->GetEquippedItemAtSlot(Slot);
    TestTrue(TEXT("New granted identity owns equipment slot"), Replacement && Replacement != Original && Replacement->IsA<USovCinematicTestEquipmentReplacement>());
    TestNull(TEXT("Removed original is not GUID-visible"), Inventory->FindItemByGUID(OriginalGUID));
    TestTrue(TEXT("All postconditions survive callbacks"), FSovCinematicTestAccess::ValidateInventory(F.Component, Error));
    FSovCinematicTestAccess::RestoreInventory(F.Component);
    TestTrue(TEXT("Rollback restores original equipment identity"), F.Pawn->GetEquipmentComponent()->GetEquippedItemAtSlot(Slot) == Original);
    TestTrue(TEXT("Rollback restores original GUID membership"), Inventory->FindItemByGUID(OriginalGUID) == Original);
    TestEqual(TEXT("Rollback preserves recharge timestamp"), Original->GetLastUseTime(), 23.f);
    TestFalse(TEXT("Rollback is complete"), FSovCinematicTestAccess::InventoryNeedsRecovery(F.Component));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicEquipmentCallbackConflictTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.LaterEquipmentOwnerAndRetryGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicEquipmentCallbackConflictTest::RunTest(const FString& Parameters)
{
    FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic"), F.Component)) { return false; }
    auto* Inventory = F.Pawn->GetInventoryComponent();
    auto OriginalAdd = Inventory->TryAddItemFromClass(USovCinematicTestEquipment::StaticClass(), 1, false);
    auto LaterAdd = Inventory->TryAddItemFromClass(USovCinematicTestEquipmentReplacement::StaticClass(), 1, false);
    if (!TestEqual(TEXT("Original equipment"), OriginalAdd.AmountGiven, 1) || !TestEqual(TEXT("Later-owner equipment"), LaterAdd.AmountGiven, 1)) { return false; }
    auto* Original = CastChecked<UEquippableItem>(OriginalAdd.Stacks[0]); auto* Later = CastChecked<UEquippableItem>(LaterAdd.Stacks[0]);
    const auto Slot = FNarrativeGameplayTags::Get().Equipment_Slot_Ammo;
    if (!Original->IsEquipped()) { Inventory->SetCinematicEquipment(Original, Slot); }
    AddCinematicReplacementManifest(F, Original); FString Error;
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    TestTrue(TEXT("Resolve replacement"), FSovCinematicTestAccess::ResolveInventory(F.Component, Error));
    auto* Probe = NewObject<USovCinematicInventoryProbe>(F.Component); Probe->Inventory = Inventory; Probe->OtherEquipment = Later;
    F.Pawn->GetEquipmentComponent()->OnItemUnequipped.AddDynamic(Probe, &USovCinematicInventoryProbe::ReplaceDuringUnequip);
    TestFalse(TEXT("Later native equip callback invalidates owned replacement"), FSovCinematicTestAccess::ApplyInventory(F.Component, Error));
    FSovCinematicTestAccess::RestoreInventory(F.Component);
    TestTrue(TEXT("Later equipment owner is preserved"), F.Pawn->GetEquipmentComponent()->GetEquippedItemAtSlot(Slot) == Later);
    TestTrue(TEXT("Conflict explicitly requires checkpoint recovery"), FSovCinematicTestAccess::InventoryNeedsRecovery(F.Component));
    TestFalse(TEXT("Conflict cannot silently retry and duplicate grants"), F.Component->RequestPlay(F.PC, Error));
    TestTrue(TEXT("Retry explains required checkpoint"), Error.Contains(TEXT("checkpoint")));
    TestFalse(TEXT("Failed scene never commits beat"), F.PC->GetCampaignState()->IsBeatComplete(F.Component->MissionId, F.Component->BeatId));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicInventoryReceiptTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.FullAndSkipShareSingleNativeReceipt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicInventoryReceiptTest::RunTest(const FString& Parameters)
{
    FManagedSequenceWorld F(true); if (!TestNotNull(TEXT("Managed cinematic"), F.Component)) { return false; }
    auto* Inventory = F.Pawn->GetInventoryComponent(); FString Error;
    // Stage the existing playback-proof boundary, then execute production native postconditions and the real campaign receipt.
    // Playback clock/binding proof itself remains covered by the existing lifecycle suite.
    for (bool Skipped : {false, true})
    {
        if (Skipped) { F.Component->BeatId = TEXT("Replay"); }
        FSovCinematicInventoryPostcondition Grant; Grant.ParticipantBinding = TEXT("Player");
        Grant.Mutation.MutationId = TEXT("ReceiptGrant"); Grant.Mutation.ItemClass = USovCinematicTestStack::StaticClass(); Grant.Mutation.Quantity = 2;
        F.Component->InventoryPostconditions = {Grant};
        FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
        if (!TestTrue(TEXT("Resolve receipt inventory manifest"), FSovCinematicTestAccess::ResolveInventory(F.Component, Error))) { return false; }
        if (!TestTrue(TEXT("Both completion paths use identical native item application"), FSovCinematicTestAccess::ApplyInventory(F.Component, Error))) { return false; }
        if (Skipped) { TestTrue(TEXT("Earlier non-skipped receipt authorizes replay skip"), F.PC->GetCampaignState()->CanSkipCinematic(TEXT("Replay"))); }
        if (!TestTrue(TEXT("Native campaign receipt commits exactly once"), FSovCinematicTestAccess::CommitInventoryReceipt(F.Component, Skipped, Error))) { return false; }
        TestFalse(TEXT("Duplicate native receipt is rejected"), FSovCinematicTestAccess::CommitInventoryReceipt(F.Component, Skipped, Error));
        const auto& Journal = F.PC->GetCampaignState()->GetJournal();
        if (!TestFalse(TEXT("Committed receipt has a journal entry"), Journal.IsEmpty())) { return false; }
        TestEqual(TEXT("Journal records correct completion path"), Journal.Last().bPresentationSkipped, Skipped);
        const int32 Quantity = Inventory->GetTotalQuantityOfItem(USovCinematicTestStack::StaticClass());
        FSovCinematicTestAccess::RestoreInventory(F.Component);
        TestEqual(TEXT("Committed inventory cannot later roll back"), Inventory->GetTotalQuantityOfItem(USovCinematicTestStack::StaticClass()), Quantity);
        FSovCinematicTestAccess::RetireInventorySession(F.Component);
    }
    TestEqual(TEXT("Exactly two grants across full and skipped receipts"), Inventory->GetTotalQuantityOfItem(USovCinematicTestStack::StaticClass()), 4);
    TestEqual(TEXT("Exactly two campaign journal receipts"), F.PC->GetCampaignState()->GetJournal().Num(), 2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicSharedWitnessTest, "ProjectVelkorran.Campaign.Cinematic.SharedCriticalWitnessRequiresActualCompanion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicSharedWitnessTest::RunTest(const FString& Parameters)
{
    FManagedSequenceWorld F(false, true); if (!TestNotNull(TEXT("Managed shared-witness cinematic"), F.Component)) { return false; }
    auto* Campaign = F.PC->GetCampaignState(); auto* Mission = Campaign->GetActiveMission();
    auto* PS = F.PC->GetPlayerState<ASovPlayerState>(); auto* Companions = F.PC->GetConvergenceCompanionState();
    const auto& Tags = FSovGameplayTags::Get(); FString Error;
    auto* SeleneDefinition = NewObject<UPlayerDefinition>(F.PC); F.PC->KeepAlive.Add(SeleneDefinition);
    FSovCampaignProtagonistProfile Alternate; Alternate.Protagonist = Tags.Character_Player_Selene;
    Alternate.PawnClass = ASovCompanionApproachTestSelene::StaticClass(); Alternate.PlayerDefinition = SeleneDefinition;
    Mission->AlternateProtagonists.Add(Alternate);
    for (bool bSelene : {false, true})
    {
        auto& Profile = Mission->ProtagonistCompanions.AddDefaulted_GetRef();
        Profile.Protagonist = bSelene ? Tags.Character_Player_Selene : Tags.Character_Player_Tarrik;
        Profile.CompanionId = bSelene ? FName(TEXT("Selene")) : FName(TEXT("Tarrik"));
        Profile.EntryAnchorTag = bSelene ? FName(TEXT("WitnessSeleneEntry")) : FName(TEXT("WitnessTarrikEntry"));
        Profile.CompanionClass = ASovCompanionApproachTestProxy::StaticClass();
        auto* NPC = NewObject<UNPCDefinition>(F.PC); F.PC->KeepAlive.Add(NPC);
        NPC->NPCClassPath = ASovCompanionApproachTestProxy::StaticClass(); Profile.CompanionDefinition = NPC;
        Mission->AllowedCompanionIds.Add(Profile.CompanionId);
    }
    auto* Evidence = NewObject<USovEvidenceDefinition>(F.PC); F.PC->KeepAlive.Add(Evidence);
    Evidence->EvidenceId = TEXT("FifthWitness"); Evidence->CanonicalContentId = TEXT("HistoricFifthWitness");
    Evidence->Summary = FText::FromString(TEXT("Both bearers observe the historical witness."));
    Evidence->OriginalCustodian = TEXT("AurelionMemory"); Evidence->SourceCustodians = {Evidence->OriginalCustodian};
    Evidence->RelevantMissions = {Mission->MissionId}; Evidence->bCriticalPath = true;
    auto& Witness = Mission->Beats[0]; Witness.BeatId = TEXT("FifthWitness"); Witness.RequiredProtagonist = Tags.Character_Player_Tarrik;
    Witness.CriticalEvidence = {Evidence}; Witness.CriticalEvidenceObserverIds = {TEXT("Tarrik"), TEXT("Selene")};
    F.Component->BeatId = Witness.BeatId;
    FSovCampaignBeatDefinition Stay; Stay.BeatId = TEXT("VoluntaryStay"); Stay.RequiredProtagonist = Tags.Character_Player_Tarrik;
    Stay.PrerequisiteBeats = {Witness.BeatId}; Mission->Beats.Add(Stay);
    if (!TestTrue(TEXT("Shared historical observation is a valid cinematic evidence contract"), Mission->ValidateDefinition(Error)))
    { AddError(Error); return false; }
    FSovCinematicParticipant Partner; Partner.BindingTag = TEXT("Selene"); Partner.ActorTag = TEXT("WitnessSelene");
    F.Component->Participants.Add(Partner);
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    TestFalse(TEXT("Missing physical Selene cannot be registered as a witness"), FSovCinematicTestAccess::ResolveParticipantSnapshot(F.Component, Error));
    FSovCinematicTestAccess::RetireInventorySession(F.Component);

    // Supply a previously played incoming kit using a real ready Selene pawn and the
    // production snapshot capture, rather than granting the observer a synthetic kit.
    auto* PriorPC = F.Base.World->SpawnActor<ASovHandoffRuntimeTestController>();
    auto* PriorPS = F.Base.World->SpawnActor<ASovPlayerState>();
    auto* PriorSelene = F.Base.World->SpawnActor<ASovCompanionApproachTestSelene>();
    if (!PriorPC || !PriorPS || !PriorSelene || !PS) { return false; }
    PriorSelene->PrepareCampaignInitialization(SeleneDefinition); PriorPC->SetTestPlayerState(PriorPS); PriorPC->Possess(PriorSelene);
    if (!TestTrue(TEXT("Prior Selene kit has actual native readiness"), PriorSelene->StageTestReadiness(PriorPS, true)
        && PriorSelene->CompleteCampaignDataInitialization(false))) { return false; }
    FSovProtagonistSnapshot SeleneKit;
    if (!TestTrue(TEXT("Previously played Selene kit is captured and retained"), PriorPS->CaptureProtagonistSnapshot(PriorSelene, SeleneKit, Error)
        && PS->StoreProtagonistSnapshot(SeleneKit))) { AddError(Error); return false; }
    PriorPC->UnPossess(); PriorSelene->Destroy(); PriorPC->Destroy(); PriorPS->Destroy();
    auto* EntryAnchor = F.Base.World->SpawnActor<AActor>(); if (!EntryAnchor) { return false; }
    EntryAnchor->Tags.Add(TEXT("WitnessSeleneEntry"));
    if (!TestTrue(TEXT("Native companion entry stages the actual inactive protagonist"), Companions->StageInitialCompanion(Mission, Tags.Character_Player_Tarrik, Error)))
    { AddError(Error); return false; }
    ASovCompanionApproachTestProxy* Selene = nullptr;
    for (TActorIterator<ASovCompanionApproachTestProxy> It(F.Base.World); It; ++It)
    { if (It->GetOwner() == F.PC) { if (Selene) { AddError(TEXT("Companion entry created duplicate proxies")); return false; } Selene = *It; } }
    if (!TestNotNull(TEXT("Actual staged Selene actor exists"), Selene)) { return false; }
    Selene->Tags.Add(Partner.ActorTag);
    auto* AI = Cast<ANarrativeNPCController>(Selene->GetController()); if (!TestNotNull(TEXT("Actual Narrative companion controller"), AI)) { return false; }
    if (!AI->HasActorBegunPlay()) { AI->DispatchBeginPlay(); }
    if (!TestTrue(TEXT("Actual native companion kit finishes initialization"), Companions->PollStaged(Error))) { AddError(Error); return false; }
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    TestFalse(TEXT("A staged but unregistered protagonist cannot be credited as an observer"), FSovCinematicTestAccess::ResolveParticipantSnapshot(F.Component, Error));
    FSovCinematicTestAccess::RetireInventorySession(F.Component);
    if (!TestTrue(TEXT("Native companion ownership commits to the current Tarrik"), Companions->CommitStaged(F.Pawn, Error))) { AddError(Error); return false; }
    TestTrue(TEXT("Committed observer is the controller-owned companion"), Companions->GetActiveCompanion() == Selene
        && Selene->GetCompanionComponent()->GetCurrentLeader() == F.Pawn);
    F.Component->Participants.Pop();
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    TestFalse(TEXT("A living companion outside the cinematic participant bindings gains no witness credit"), FSovCinematicTestAccess::ResolveParticipantSnapshot(F.Component, Error));
    F.Component->Participants.Add(Partner);
    if (!TestTrue(TEXT("Both actual bound protagonists satisfy the shared observer contract"), FSovCinematicTestAccess::ResolveParticipantSnapshot(F.Component, Error)))
    { AddError(Error); return false; }
    if (!TestTrue(TEXT("Native empty inventory postconditions resolve and apply"), FSovCinematicTestAccess::ResolveInventory(F.Component, Error)
        && FSovCinematicTestAccess::ApplyInventory(F.Component, Error))) { AddError(Error); return false; }
    // Reuse the existing suite's accepted playback boundary; evidence, observer
    // admission, native postconditions and campaign receipt all execute in production.
    if (!TestTrue(TEXT("Accepted native cinematic receipt commits the historical witness"), FSovCinematicTestAccess::CommitInventoryReceipt(F.Component, false, Error)))
    { AddError(Error); return false; }
    TestTrue(TEXT("Controlled Tarrik knows the Fifth Witness"), Campaign->KnowsEvidence(Evidence->EvidenceId, Tags.Character_Player_Tarrik));
    TestTrue(TEXT("Actual bound companion Selene knows the same Fifth Witness"), Campaign->KnowsEvidence(Evidence->EvidenceId, Tags.Character_Player_Selene));
    TestFalse(TEXT("Shared observation does not silently choose VoluntaryStay"), Campaign->IsBeatComplete(Mission->MissionId, TEXT("VoluntaryStay")));
    TestEqual(TEXT("The shared observation is one provenance record"), Campaign->GetEvidence().Num(), 1);
    FSovCinematicTestAccess::RestoreInventory(F.Component); FSovCinematicTestAccess::RetireInventorySession(F.Component);
    const auto SerializeCampaign = [Campaign]()
    {
        TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        Archive.ArIsSaveGame = true; Campaign->Serialize(Archive); return Bytes;
    };
    TestTrue(TEXT("Accepted shared observation survives native journal replay validation"), USovCampaignStateComponent::ValidateSerializedSave(SerializeCampaign(), Error));
    if (Campaign->GetEvidence().IsEmpty()) { return false; }
    auto& SavedEvidence = const_cast<FSovEvidenceAcquisition&>(Campaign->GetEvidence()[0]);
    const auto AcceptedObservers = SavedEvidence.WitnessIds;
    SavedEvidence.WitnessIds.Remove(TEXT("Selene"));
    TestFalse(TEXT("Dropping a required historical observer corrupts the restore contract"), USovCampaignStateComponent::ValidateSerializedSave(SerializeCampaign(), Error));
    SavedEvidence.WitnessIds = AcceptedObservers; SavedEvidence.WitnessIds.Add(TEXT("UnboundObserver"));
    TestFalse(TEXT("Restore rejects invented observer credit"), USovCampaignStateComponent::ValidateSerializedSave(SerializeCampaign(), Error));
    SavedEvidence.WitnessIds = AcceptedObservers;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicInventoryUnregisterTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.UnregisterDuringGrantRestoresOwnedDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicInventoryUnregisterTest::RunTest(const FString& Parameters)
{
    FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic"), F.Component)) { return false; }
    auto* Inventory = F.Pawn->GetInventoryComponent(); const int32 Before = Inventory->GetItems().Num(); FString Error;
    FSovCinematicInventoryPostcondition Grant; Grant.ParticipantBinding = TEXT("Player"); Grant.Mutation.MutationId = TEXT("CanceledGrant");
    Grant.Mutation.ItemClass = USovCinematicTestStack::StaticClass(); F.Component->InventoryPostconditions = {Grant};
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    TestTrue(TEXT("Resolve pending inventory"), FSovCinematicTestAccess::ResolveInventory(F.Component, Error));
    auto* Probe = NewObject<USovCinematicInventoryProbe>(F.Component); Probe->Managed = F.Component;
    Inventory->OnItemAdded.AddDynamic(Probe, &USovCinematicInventoryProbe::RetireDuringAdd);
    TestFalse(TEXT("Unregister invalidates commit request"), FSovCinematicTestAccess::ApplyInventory(F.Component, Error));
    FSovCinematicTestAccess::RestoreInventory(F.Component);
    TestEqual(TEXT("Owned grant is removed without touching prior inventory"), Inventory->GetItems().Num(), Before);
    TestFalse(TEXT("Interrupted scene does not commit beat"), F.PC->GetCampaignState()->IsBeatComplete(F.Component->MissionId, F.Component->BeatId));
    TestFalse(TEXT("Input ownership released"), F.PC->IsMoveInputIgnored());
    return true;
}

namespace SovRequiredCharacterSequenceTests
{
	struct FWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovRequiredCharacterSequenceTestActor* Actor = nullptr;
		ASovNPCVisualLifecycleTestCharacter* Character = nullptr;
		ASovNPCVisualLifecycleTestVisual* Visual = nullptr;
		UNPCDefinition* Definition = nullptr;
		UNarrativeAbilitySystemComponent* ASC = nullptr;
		USovSequenceLifecycleProbe* Probe = nullptr;
		ULevelSequence* Sequence = nullptr;
		FWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeActorsForPlay(FURL());
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<ASovNPCVisualLifecycleTestCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			Actor = World->SpawnActor<ASovRequiredCharacterSequenceTestActor>();
			if (!Character || !Actor) { return; }
			Definition = NewObject<UNPCDefinition>(Character);
			Definition->NPCID = TEXT("RequiredSequenceCharacterFixture");
			Definition->bAllowMultipleInstances = true;
			Definition->NPCClassPath = ASovNPCVisualLifecycleTestCharacter::StaticClass();
			// The authored Enforcer, tracked under Content/Aurelion/ with fully tracked transitive
			// dependencies, exercises native attribute/startup/ability grants with shipped data.
			// Only appearance IO is replaced by the existing empty-mesh producer fixture.
			const auto* Seed = LoadObject<UNPCDefinition>(nullptr,
				SovTrackedContentPaths::AuthoredEnforcerDefinition);
			if (!Seed || !Seed->AbilityConfiguration) { return; }
			Definition->AbilityConfiguration = Seed->AbilityConfiguration;
			Character->AuthoredPlacedDefinition = Definition;
			ASC = Character->GetNarrativeAbilitySystemComponent();
			Sequence = NewObject<ULevelSequence>(Actor); Sequence->Initialize();
			auto* Scene = Sequence->GetMovieScene(); Scene->SetPlaybackRange(0, 240000);
			const FGuid Binding = Scene->AddPossessable(TEXT("RequiredCharacter"), ANarrativeCharacter::StaticClass());
			Scene->TagBinding(TEXT("RequiredCharacter"), UE::MovieScene::FFixedObjectBindingID(Binding, MovieSceneSequenceID::Root));
			Actor->InitializeTestSequence(Sequence);
			FNarrativeSequencePlaybackSettings Settings;
			Settings.bAutoPlay = false; Settings.RequiredParticipantBindingTags.Add(TEXT("RequiredCharacter"));
			Actor->UpdateSequence(Sequence, Settings);
			Actor->SetBindingByTag(TEXT("RequiredCharacter"), {Character});
			Probe = NewObject<USovSequenceLifecycleProbe>(Actor);
			Actor->OnPlaybackFailed.AddDynamic(Probe, &USovSequenceLifecycleProbe::Failed);
		}
		~FWorld()
		{
			if (Actor && IsValid(Actor)) { Actor->GetSequencePlayer()->Stop(); Actor->Destroy(); }
			if (Character && IsValid(Character)) { Character->Destroy(); }
			if (Visual && IsValid(Visual)) { Visual->Destroy(); }
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		bool Valid() const { return World && Actor && Character && Definition && Definition->AbilityConfiguration && ASC && Probe; }
		void InitializeCharacter() { Character->DispatchBeginPlay(); }
		void CreateVisual()
		{
			FActorSpawnParameters Params; Params.Owner = Character;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Visual = World->SpawnActor<ASovNPCVisualLifecycleTestVisual>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (Visual) { Visual->SetCharacterForTest(Character); Character->SetVisualForTest(Visual); }
		}
		void Play() { Actor->GetSequencePlayer()->Play(); }
		void Stop() { Actor->GetSequencePlayer()->Stop(); }
		bool IsPlaying() const { return Actor->GetSequencePlayer()->IsPlaying(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRealConfiguredCinematicParticipantTest,
	"ProjectVelkorran.Campaign.Cinematic.RequiredCharacterUsesActualNativeStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovRealConfiguredCinematicParticipantTest::RunTest(const FString& Parameters)
{
	using namespace SovRequiredCharacterSequenceTests;
	FWorld Scope;
	if (!TestTrue(TEXT("Real config, native character and tagged sequence prerequisites"), Scope.Valid())) { return false; }
	TestFalse(TEXT("No manually applied startup effects"), Scope.ASC->bStartupEffectsApplied);
	Scope.InitializeCharacter();
	TestTrue(TEXT("Real character BeginPlay ran"), Scope.Character->HasActorBegunPlay());
	TestTrue(TEXT("Real AddStartupEffects completed the authored configuration"), Scope.ASC->bStartupEffectsApplied);
	TestFalse(TEXT("Unmaintained legacy field remains false; no test seeding"), Scope.ASC->bInitializedFromConfig);
	TestTrue(TEXT("Actual default attribute effect supplied living health"), Scope.Character->GetHealth() > 0.f);
	int32 GrantedAbilities = 0;
	for (const auto& Ability : Scope.Definition->AbilityConfiguration->DefaultAbilities)
	{
		// Unset entries cannot be granted; see SovPlacedNPCDefinitionRuntimeTests for the shared
		// configuration that carries them.
		if (!Ability.Get()) { continue; }
		const auto* Spec = Scope.ASC->FindAbilitySpecFromClass(Ability);
		if (TestNotNull(*FString::Printf(TEXT("Actual startup ability %s was granted"), *GetNameSafe(Ability.Get())), Spec))
		{
			++GrantedAbilities;
			TestTrue(TEXT("Grant retains its actual configuration source"), Spec->SourceObject.Get() == Scope.Definition->AbilityConfiguration);
		}
	}
	TestTrue(TEXT("At least one real startup ability was granted"), GrantedAbilities > 0);
	Scope.CreateVisual();
	if (!TestNotNull(TEXT("Current owned visual"), Scope.Visual)) { return false; }
	Scope.Visual->CompleteMeshesForTest();
	TestFalse(TEXT("Real visual producer completed pending-load state"), Scope.Character->IsCharacterPendingLoad());
	const float Health = Scope.Character->GetHealth();
	TestTrue(TEXT("Required actor binding exists without any transform track"),
		Scope.Sequence->GetMovieScene()->GetBindings().Num() == 1 && Scope.Sequence->GetMovieScene()->GetBindings()[0].GetTracks().IsEmpty());
	Scope.Play();
	TestTrue(TEXT("Real SequencePlayer.OnPlay admits the configured required character"), Scope.IsPlaying());
	TestEqual(TEXT("No permanent-false participant failure"), Scope.Probe->FailedCount, 0);
	TestTrue(TEXT("Real binding resolver returns the exact actor"), Scope.Actor->GetBoundObjects().Contains(Scope.Character));
	TestTrue(TEXT("Native participant ownership acquired"), Scope.ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled));
	Scope.Stop();
	TestFalse(TEXT("Normal stop releases only the cinematic-owned tag"), Scope.ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled));
	TestEqual(TEXT("Playback admission never rewrites health"), Scope.Character->GetHealth(), Health);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRequiredCinematicParticipantRefusalTest,
	"ProjectVelkorran.Campaign.Cinematic.RequiredCharacterStillRefusesUnreadyOrRetiredState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovRequiredCinematicParticipantRefusalTest::RunTest(const FString& Parameters)
{
	using namespace SovRequiredCharacterSequenceTests;
	FWorld Scope;
	if (!TestTrue(TEXT("Real required participant prerequisites"), Scope.Valid())) { return false; }
	auto Refused = [this, &Scope](const TCHAR* Reason)
	{
		const int32 Before = Scope.Probe->FailedCount;
		Scope.Play();
		TestFalse(Reason, Scope.IsPlaying());
		TestEqual(TEXT("Native failure delegate fired exactly once"), Scope.Probe->FailedCount, Before + 1);
		TestFalse(TEXT("Rejected participant acquired no cinematic lease"), Scope.ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled));
	};
	Refused(TEXT("Before native startup no required participant admission"));
	Scope.InitializeCharacter();
	Refused(TEXT("Configured character without its visual is refused"));
	Scope.CreateVisual();
	if (!TestNotNull(TEXT("Actual pending visual"), Scope.Visual)) { return false; }
	TestTrue(TEXT("Uncompleted visual is genuinely pending"), Scope.Character->IsCharacterPendingLoad());
	Refused(TEXT("Pending base meshes still refuse playback"));
	Scope.Visual->CompleteMeshesForTest();
	Scope.Play();
	if (!TestTrue(TEXT("Same actor admits after real initialization/visual completion"), Scope.IsPlaying())) { return false; }
	Scope.Stop();
	FActorSpawnParameters AvatarParams;
	AvatarParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* ReplacementAvatar = Scope.World->SpawnActor<ASovNPCVisualLifecycleTestCharacter>(
		FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator, AvatarParams);
	if (!TestNotNull(TEXT("Distinct Narrative character avatar"), ReplacementAvatar)) { return false; }
	Scope.ASC->InitAbilityActorInfo(Scope.Character, ReplacementAvatar);
	if (!TestTrue(TEXT("Native ASC actually accepted the replacement avatar"), Scope.ASC->GetAvatarActor() == ReplacementAvatar)) { return false; }
	Refused(TEXT("A replaced ASC avatar still refuses playback"));
	Scope.ASC->InitAbilityActorInfo(Scope.Character, Scope.Character);
	if (!TestTrue(TEXT("Original ASC avatar restored before later refusal cases"), Scope.ASC->GetAvatarActor() == Scope.Character)) { return false; }
	const auto Configuration = Scope.Definition->AbilityConfiguration;
	Scope.Definition->AbilityConfiguration = nullptr;
	Refused(TEXT("Current definition without its configuration refuses playback"));
	Scope.Definition->AbilityConfiguration = Configuration;
	const FGameplayTag Dead = FNarrativeGameplayTags::Get().State_IsDead;
	Scope.ASC->AddLooseGameplayTag(Dead);
	Refused(TEXT("Actual native StopTags retain the dead-character veto"));
	TestTrue(TEXT("Rejected playback preserves an external stop tag"), Scope.ASC->HasMatchingGameplayTag(Dead));
	Scope.ASC->RemoveLooseGameplayTag(Dead);
	Scope.ASC->ClearTrackedStartupEffects();
	TestFalse(TEXT("Actual startup-effect retirement invalidates maintained completion"), Scope.ASC->bStartupEffectsApplied);
	Refused(TEXT("Retired authoritative startup configuration refuses playback"));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicDetachedBodyExitTest,
    "ProjectVelkorran.Campaign.Cinematic.DetachedBodyCannotAdmitRootOnlyExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicDetachedBodyExitTest::RunTest(const FString& Parameters)
{
    FManagedSequenceWorld F;
    if (!TestNotNull(TEXT("Managed cinematic fixture"), F.Component)) { return false; }
    auto* Mesh = F.Pawn->GetMesh();
    if (!TestNotNull(TEXT("Actual character skeletal component"), Mesh)) { return false; }
    FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
    F.Component->Participants[0].bApplyExitTransform = true;
    F.Component->Participants[0].ExitTransform = F.Pawn->GetActorTransform();
    const FTransform OriginalRoot = F.Pawn->GetActorTransform(), OriginalMesh = Mesh->GetRelativeTransform();
    auto* Parent = Mesh->GetAttachParent();
    if (!TestNotNull(TEXT("Root-owned mesh parent"), Parent)) { return false; }
    const float Health = F.Pawn->GetHealth();
    FString Error;
    TestTrue(TEXT("Attached non-ragdolled participant admits exit ownership"), FSovCinematicTestAccess::ValidateParticipantState(F.Component, Error));
    Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    TestFalse(TEXT("A detached body refuses root-only exit admission"), FSovCinematicTestAccess::ValidateParticipantState(F.Component, Error));
    TestTrue(TEXT("Refusal identifies body ownership"), Error.Contains(TEXT("attached, non-ragdolled")));
    TestNull(TEXT("Refusal does not forcibly reattach another body owner"), Mesh->GetAttachParent());
    F.Component->Participants[0].bApplyExitTransform = false;
    TestTrue(TEXT("Presentation without root relocation keeps its existing admission policy"), FSovCinematicTestAccess::ValidateParticipantState(F.Component, Error));
    F.Component->Participants[0].bApplyExitTransform = true;
    Mesh->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform); Mesh->SetRelativeTransform(OriginalMesh);
    TestTrue(TEXT("Normal attached ownership admits again"), FSovCinematicTestAccess::ValidateParticipantState(F.Component, Error));
    const FGameplayTag Ragdoll = FNarrativeGameplayTags::Get().State_Movement_Ragdoll;
    F.ASC->AddLooseGameplayTag(Ragdoll);
    TestFalse(TEXT("Native ragdoll state also vetoes transform ownership"), FSovCinematicTestAccess::ValidateParticipantState(F.Component, Error));
    TestTrue(TEXT("Refusal preserves the external ragdoll tag"), F.ASC->HasMatchingGameplayTag(Ragdoll));
    F.ASC->RemoveLooseGameplayTag(Ragdoll);
    TestTrue(TEXT("Neither refusal moves the actor"), F.Pawn->GetActorTransform().Equals(OriginalRoot));
    TestEqual(TEXT("Neither refusal changes health"), F.Pawn->GetHealth(), Health);
    TestTrue(TEXT("No scene receipt is created by state validation"), F.PC->GetCampaignState()->GetJournal().IsEmpty());
    FSovCinematicTestAccess::RetireInventorySession(F.Component);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicSkipRoutingTest, "ProjectVelkorran.Campaign.Cinematic.SkipAndPauseReachThePlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicSkipRoutingTest::RunTest(const FString& Parameters)
{
	// RequestSkip and SetCinematicPaused were reachable only from tests: no input reached skip, and a
	// pause menu froze the world while the scene's own player kept running (audit UX2-06).
	FManagedSequenceWorld F; if (!TestNotNull(TEXT("Managed cinematic component"), F.Component)) { return false; }
	// The controller subscribes to its own semantic input when it begins play, as it does in a level.
	if (!F.PC->HasActorBegunPlay()) { F.PC->DispatchBeginPlay(); }
	const FGameplayTag SkipTag = FSovGameplayTags::Get().Input_SkipCinematic;
	FString Error;
	TestFalse(TEXT("With no scene on screen there is nothing to skip"), F.PC->RequestCinematicSkip(Error));
	TestNull(TEXT("No scene is published before one plays"), F.PC->GetActiveCinematic());
	F.PC->OnSemanticInputChanged.Broadcast(SkipTag, true);
	TestFalse(TEXT("Holding skip outside a scene starts nothing"), FSovCinematicSkipTestAccess::HoldRunning(F.PC));

	FSovCinematicTestAccess::StageOwnedSession(F.Component, F.PC, F.Pawn, F.ASC);
	F.Base.Actor->GetSequencePlayer()->Play();
	FSovCinematicTestAccess::StagePlaying(F.Component, F.Base.Actor->GetPlaybackGeneration());
	TestEqual(TEXT("A playing scene publishes itself to its player"), F.PC->GetActiveCinematic(), F.Component);

	// A pause menu has to stop the sequence as well: world pause does not reach its clock, and a scene
	// that ran on past the pause would fail its own progress check and lose the first viewing. This
	// transient world has no game mode, so the system pause owner's own call is made directly.
	FSovCinematicSkipTestAccess::PauseAsPauseOwnerWould(F.PC, true);
	TestTrue(TEXT("Pausing the game pauses the scene's own player"), F.Base.Actor->GetSequencePlayer()->IsPaused());
	TestEqual(TEXT("The component records the pause"), F.Component->GetPhase(), ESovCinematicPhase::Paused);
	FSovCinematicSkipTestAccess::PauseAsPauseOwnerWould(F.PC, false);
	TestFalse(TEXT("Releasing the pause resumes the scene"), F.Base.Actor->GetSequencePlayer()->IsPaused());
	TestEqual(TEXT("The component records the resume"), F.Component->GetPhase(), ESovCinematicPhase::Playing);

	// Skip now routes to the component, which still owns the rule: a first viewing cannot be skipped.
	TestFalse(TEXT("A scene never seen in full cannot be skipped"), F.PC->RequestCinematicSkip(Error));
	TestTrue(TEXT("The refusal explains itself"), Error.Contains(TEXT("prior complete viewing")));

	// Skipping is a hold, so a stray press must not consume a scene.
	F.PC->OnSemanticInputChanged.Broadcast(SkipTag, true);
	TestTrue(TEXT("Holding skip during a scene starts the hold"), FSovCinematicSkipTestAccess::HoldRunning(F.PC));
	TestEqual(TEXT("The press alone changes nothing"), F.Component->GetPhase(), ESovCinematicPhase::Playing);
	F.PC->OnSemanticInputChanged.Broadcast(SkipTag, false);
	TestFalse(TEXT("Letting go cancels the hold"), FSovCinematicSkipTestAccess::HoldRunning(F.PC));
	F.PC->OnSemanticInputChanged.Broadcast(FSovGameplayTags::Get().Input_ThreatFocus, true);
	TestFalse(TEXT("An unrelated input never starts a skip"), FSovCinematicSkipTestAccess::HoldRunning(F.PC));

	// A scene that leaves the screen stops being skippable, and takes any live hold with it.
	F.PC->OnSemanticInputChanged.Broadcast(SkipTag, true);
	F.Component->Abort(TEXT("Test teardown"));
	TestNull(TEXT("A retired scene unpublishes itself"), F.PC->GetActiveCinematic());
	TestFalse(TEXT("A retired scene cancels its hold"), FSovCinematicSkipTestAccess::HoldRunning(F.PC));
	TestFalse(TEXT("Skip finds nothing once the scene is gone"), F.PC->RequestCinematicSkip(Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovManagedPendingVisualStartupTest,
	"ProjectVelkorran.Campaign.Cinematic.ManagedWaitsForRealVisualProducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovManagedPendingVisualStartupTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F;
	if (!TestTrue(TEXT("Gameplay-ready player with native pending appearance"), F.PreparePendingVisual())) { return false; }
	FSovCinematicTestAccess::StartPrepared(F.Component);
	TestEqual(TEXT("Preparation remains Loading"), F.Component->GetPhase(), ESovCinematicPhase::Loading);
	TestTrue(TEXT("Native actor queues readiness"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Base.Actor));
	TestFalse(TEXT("Player has not started"), F.Base.Actor->GetSequencePlayer()->IsPlaying());
	TestFalse(TEXT("Waiting cannot issue proof"), FSovCinematicTestAccess::HasReceipt(F.Component));
	F.Base.Actor->Tick(.01f);
	TestEqual(TEXT("No premature OnPlay"), F.Base.Probe->StartedCount, 0);
	F.PendingVisual->CompleteMeshesForTest();
	TestFalse(TEXT("Actual visual completion retires pending load"), F.Pawn->IsCharacterPendingLoad());
	F.Base.Actor->Tick(.01f);
	TestEqual(TEXT("Managed callback enters Playing"), F.Component->GetPhase(), ESovCinematicPhase::Playing);
	TestTrue(TEXT("Real resolved binding retains controlled pawn"), F.Base.Actor->GetBoundObjects().Contains(F.Pawn));
	F.Base.Actor->Tick(.01f);
	TestEqual(TEXT("Ready transition starts exactly once"), F.Base.Probe->StartedCount, 1);
	TestFalse(TEXT("Starting alone cannot issue proof"), FSovCinematicTestAccess::HasReceipt(F.Component));
	F.Component->Abort(TEXT("Test teardown"));
	TestFalse(TEXT("Teardown releases managed ownership"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovManagedPendingCancellationTest,
	"ProjectVelkorran.Campaign.Cinematic.ManagedPendingCancellationAndWatchdog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovManagedPendingCancellationTest::RunTest(const FString& Parameters)
{
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FManagedSequenceWorld F;
		if (!TestTrue(TEXT("Pending appearance prerequisites"), F.PreparePendingVisual())) { return false; }
		F.Base.Actor->NarrativeSequenceParams.ParticipantReadyTimeoutSeconds = .1f;
		FSovCinematicTestAccess::StartPrepared(F.Component);
		TestTrue(TEXT("Real preparation queued playback"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Base.Actor));
		if (Mode == 0) { F.Component->Abort(TEXT("Player canceled during load")); }
		else if (Mode == 1) { FSovCinematicTestAccess::ExpireLoading(F.Component); }
		else { F.Base.Actor->Tick(.11f); }
		TestEqual(TEXT("Cancellation or timeout is terminal"), F.Component->GetPhase(), ESovCinematicPhase::Failed);
		TestFalse(TEXT("Terminal path cancels queued playback"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Base.Actor));
		TestFalse(TEXT("Terminal path releases leases and receipt"), FSovCinematicTestAccess::OwnsAnything(F.Component));
		TestFalse(TEXT("Movement input released"), F.PC->IsMoveInputIgnored());
		TestFalse(TEXT("Look input released"), F.PC->IsLookInputIgnored());
		F.PendingVisual->CompleteMeshesForTest(); F.Base.Actor->Tick(.2f); F.Base.Actor->Tick(.2f);
		TestFalse(TEXT("Late visual completion cannot start canceled scene"), F.Base.Actor->GetSequencePlayer()->IsPlaying());
		TestEqual(TEXT("No late OnPlay"), F.Base.Probe->StartedCount, 0);
		TestEqual(TEXT("Participant timeout emits exactly one failure"), F.Base.Probe->FailedCount, Mode == 2 ? 1 : 0);
		TestFalse(TEXT("No completion receipt after cancellation"), FSovCinematicTestAccess::HasReceipt(F.Component));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPendingGenerationCancellationTest,
	"ProjectVelkorran.Campaign.Cinematic.PendingCancellationPreservesReplacementGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPendingGenerationCancellationTest::RunTest(const FString& Parameters)
{
	FManagedSequenceWorld F;
	if (!TestTrue(TEXT("Pending appearance prerequisites"), F.PreparePendingVisual())) { return false; }
	FSovCinematicTestAccess::StartPrepared(F.Component);
	const uint64 OldGeneration = F.Base.Actor->GetPlaybackGeneration();
	auto Settings = F.Base.Actor->NarrativeSequenceParams;
	Settings.bAutoPlay = false;
	F.Base.Actor->UpdateSequence(F.Base.Sequence, Settings);
	F.Base.Actor->PlaySequence();
	const uint64 NewGeneration = F.Base.Actor->GetPlaybackGeneration();
	TestTrue(TEXT("Replacement reserves a newer generation"), NewGeneration > OldGeneration);
	TestTrue(TEXT("Replacement waits for same pending visual"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Base.Actor));
	F.Base.Actor->CancelPendingPlayback(OldGeneration);
	F.Component->Abort(TEXT("Old owner canceled"));
	TestTrue(TEXT("Old owner cannot cancel replacement"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Base.Actor));
	F.PendingVisual->CompleteMeshesForTest(); F.Base.Actor->Tick(.01f);
	TestTrue(TEXT("Replacement can still start"), F.Base.Actor->GetSequencePlayer()->IsPlaying());
	TestEqual(TEXT("Replacement starts once"), F.Base.Probe->StartedCount, 1);
	TestFalse(TEXT("Old component issues no receipt"), FSovCinematicTestAccess::HasReceipt(F.Component));
	F.Base.Actor->GetSequencePlayer()->Stop();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReadyManagedStartupReentryTest,
	"ProjectVelkorran.Campaign.Cinematic.ReadyManagedStartupAndReentrantAbort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovReadyManagedStartupReentryTest::RunTest(const FString& Parameters)
{
	for (bool bAbortOnPlay : {false, true})
	{
		FManagedSequenceWorld F;
		if (!TestTrue(TEXT("Appearance producer prerequisites"), F.PreparePendingVisual())) { return false; }
		F.PendingVisual->CompleteMeshesForTest();
		if (bAbortOnPlay)
		{
			F.Base.Probe->Managed = F.Component;
			F.Base.Actor->GetSequencePlayer()->OnPlay.AddDynamic(F.Base.Probe, &USovSequenceLifecycleProbe::AbortManaged);
		}
		FSovCinematicTestAccess::StartPrepared(F.Component);
		TestEqual(TEXT("Ready path starts once"), F.Base.Probe->StartedCount, 1);
		TestEqual(TEXT("Reentrant abort is preserved by preparation"), F.Component->GetPhase(),
			bAbortOnPlay ? ESovCinematicPhase::Failed : ESovCinematicPhase::Playing);
		if (!bAbortOnPlay) { F.Component->Abort(TEXT("Test teardown")); }
		F.Base.Actor->Tick(.2f);
		TestFalse(TEXT("No orphan queued request"), FNarrativeSequenceLifecycleTestAccess::Pending(F.Base.Actor));
		TestFalse(TEXT("No late playback after teardown"), F.Base.Actor->GetSequencePlayer()->IsPlaying());
		TestFalse(TEXT("No leaked managed ownership"), FSovCinematicTestAccess::OwnsAnything(F.Component));
	}
	return true;
}
#endif
