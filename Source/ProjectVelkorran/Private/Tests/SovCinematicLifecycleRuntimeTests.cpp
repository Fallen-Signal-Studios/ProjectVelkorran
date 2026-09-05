// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCinematicLifecycleRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCinematicInventoryRuntimeTestFixtures.h"
#include "Components/EquipmentComponent.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/PlayerDefinition.h"
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
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "TimerManager.h"

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
struct FSovCinematicTestAccess
{
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
	{ C->Phase = ESovCinematicPhase::Playing; C->ExpectedPlaybackGeneration = Generation; }
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
		UWorld* World = nullptr;
		ASovSequenceLifecycleTestActor* Actor = nullptr;
		ASovAxiomRuntimeTestCharacter* Participant = nullptr;
		ULevelSequence* Sequence = nullptr;
		UNarrativeAbilitySystemComponent* ASC = nullptr;
		USovSequenceLifecycleProbe* Probe = nullptr;
		FSequenceWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));
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
		FManagedSequenceWorld(bool bIncludeReplayBeat = false)
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
			PC->SetViewTarget(Pawn);
			auto* Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
			Mission->MissionId = TEXT("CinematicRuntime"); Mission->Protagonist = Pawn->GetProtagonistIdentityTag();
			Mission->PawnClass = Pawn->GetClass(); Mission->PlayerDefinition = Definition;
			FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("Scene"); Beat.CinematicId = TEXT("FirstView"); Beat.bRequiresCinematicProof = true;
			Mission->Beats.Add(Beat);
			if (bIncludeReplayBeat)
			{ FSovCampaignBeatDefinition Replay = Beat; Replay.BeatId = TEXT("Replay"); Replay.PrerequisiteBeats = {Beat.BeatId}; Mission->Beats.Add(Replay); }
			if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { return; }
			Component = NewObject<USovCampaignCinematicComponent>(Base.Actor); Base.Actor->AddInstanceComponent(Component); Component->RegisterComponent();
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
	F.Component->Participants.Add(F.Component->Participants[0]);
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
	TestFalse(TEXT("Pause cannot succeed after its callback aborts the request"), F.Component->SetCinematicPaused(true));
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
        TestTrue(TEXT("Resolve receipt inventory manifest"), FSovCinematicTestAccess::ResolveInventory(F.Component, Error));
        TestTrue(TEXT("Both completion paths use identical native item application"), FSovCinematicTestAccess::ApplyInventory(F.Component, Error));
        if (Skipped) { TestTrue(TEXT("Earlier non-skipped receipt authorizes replay skip"), F.PC->GetCampaignState()->CanSkipCinematic(TEXT("Replay"))); }
        TestTrue(TEXT("Native campaign receipt commits exactly once"), FSovCinematicTestAccess::CommitInventoryReceipt(F.Component, Skipped, Error));
        TestFalse(TEXT("Duplicate native receipt is rejected"), FSovCinematicTestAccess::CommitInventoryReceipt(F.Component, Skipped, Error));
        TestEqual(TEXT("Journal records correct completion path"), F.PC->GetCampaignState()->GetJournal().Last().bPresentationSkipped, Skipped);
        const int32 Quantity = Inventory->GetTotalQuantityOfItem(USovCinematicTestStack::StaticClass());
        FSovCinematicTestAccess::RestoreInventory(F.Component);
        TestEqual(TEXT("Committed inventory cannot later roll back"), Inventory->GetTotalQuantityOfItem(USovCinematicTestStack::StaticClass()), Quantity);
        FSovCinematicTestAccess::RetireInventorySession(F.Component);
    }
    TestEqual(TEXT("Exactly two grants across full and skipped receipts"), Inventory->GetTotalQuantityOfItem(USovCinematicTestStack::StaticClass()), 4);
    TestEqual(TEXT("Exactly two campaign journal receipts"), F.PC->GetCampaignState()->GetJournal().Num(), 2);
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
#endif
