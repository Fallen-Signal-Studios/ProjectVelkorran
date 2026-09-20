// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCampaignTerminalRuntimeTestFixtures.h"
#include "Tests/SovAurelionRequestRuntimeTestFixtures.h"
#include "Tests/SovCoActionRuntimeTestFixtures.h"
#include "Tests/SovCompanionCommandTestFixtures.h"
#include "NarrativeGameplayTags.h"
#include "Companions/SovCompanionCommandActivity.h"
#include "Tests/SovAurelionThermalTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Interaction/InteractionSubsystem.h"
#include "Campaign/SovAurelionRequestActor.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "UObject/Script.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#include "Resonance/SovResonancePolicy.h"
#include <limits>
#if WITH_AUTOMATION_TESTS
namespace
{
    struct FTerminalWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovHandoffRuntimeTestController* PC = nullptr;
        ASovHandoffRuntimeTestPawn* Player = nullptr;
        UNarrativeAbilitySystemComponent* ASC = nullptr;
        USovCampaignDefinition* Mission = nullptr;
        USovCampaignTerminalTestInteraction* Interaction = nullptr;
        ASovAurelionStoryTestActor* Story = nullptr;
        ASovAurelionRequestActor* Terminal = nullptr;
        uint64 Frame = GFrameCounter;
        FTerminalWorld()
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeActorsForPlay(FURL());
            World->GetTimerManager().Tick(0.f);
            PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
            Player = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
            auto* PS = World->SpawnActor<ASovPlayerState>();
            if (!PC || !Player || !PS) { return; }
            World->AddController(PC);
            auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            Player->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Player);
            if (!Player->StageTestReadiness(PS, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
            ASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
            Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
            Mission->MissionId = TEXT("M12_FireAndFrost"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
            Mission->PawnClass = ASovHandoffRuntimeTestPawn::StaticClass(); Mission->PlayerDefinition = Definition;
            FSovCampaignBeatDefinition Operate; Operate.BeatId = TEXT("Operate"); Operate.RequiredProtagonist = Mission->Protagonist;
            Operate.bRequiresCinematicProof = true; Operate.CinematicId = TEXT("RequestScene"); Operate.ObjectiveText = FText::FromString(TEXT("Operate the test terminal"));
            FSovCampaignBeatDefinition Exit; Exit.BeatId = TEXT("Exit"); Exit.PrerequisiteBeats = { Operate.BeatId };
            Exit.ObjectiveText = FText::FromString(TEXT("Reach the exit"));
            Mission->Beats = { Operate, Exit };
            if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { ASC = nullptr; return; }
            Terminal = World->SpawnActor<ASovAurelionRequestActor>();
            Terminal->RequestId = TEXT("TerminalA"); Terminal->MissionId = Mission->MissionId; Terminal->BeatId = Operate.BeatId;
            Terminal->Operation = ESovAurelionRequest::PlayScene;
            Terminal->SetActorLocation(Player->GetActorLocation() + FVector(180, 0, 0));
            Story = World->SpawnActor<ASovAurelionStoryTestActor>();
            Story->SetActorLocation(Terminal->GetActorLocation());
            auto* Sequence = NewObject<ULevelSequence>(PC); PC->KeepAlive.Add(Sequence); Sequence->Initialize(); Sequence->GetMovieScene()->SetPlaybackRange(0, 240000);
            Story->InitializeTestSequence(Sequence); Story->CampaignCinematic->Sequence = Sequence;
            Story->CampaignCinematic->MissionId = Mission->MissionId; Story->CampaignCinematic->BeatId = Operate.BeatId;
            FSovCinematicParticipant Participant; Participant.BindingTag = TEXT("Hero"); Participant.bControlledProtagonist = true; Story->CampaignCinematic->Participants = { Participant };
            FSovAurelionDialogueCue Cue; Cue.Speaker = FText::FromString(TEXT("Tarrik")); Cue.Text = FText::FromString(TEXT("We will observe the boundary.")); Cue.DurationSeconds = 4.f; Story->DialogueCues = { Cue };
            Story->CampaignCinematic->Activate(); Terminal->Story = Story;
            Interaction = NewObject<USovCampaignTerminalTestInteraction>(PC);
            PC->AddInstanceComponent(Interaction); Interaction->RegisterComponent(); Interaction->Configure(PC);
            Interaction->Activate(); Interaction->SetViewedInteractable(Terminal->Interactable);
        }
        ~FTerminalWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        void NextFrame()
        { TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame); World->GetTimerManager().Tick(.016f); }
        AActor* Blocker()
        {
            auto* Actor = World->SpawnActor<AActor>(); auto* Shape = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Shape); Actor->AddInstanceComponent(Shape); Shape->SetBoxExtent(FVector(15, 150, 200));
            Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Shape->SetCollisionResponseToAllChannels(ECR_Block);
            Shape->RegisterComponent(); Actor->SetActorLocation(Player->GetActorLocation() + FVector(90, 0, 0)); return Actor;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionRequestHoldTest, "ProjectVelkorran.Campaign.Aurelion.Request.NativeHoldRetirementCannotSupplySceneProof",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionRequestHoldTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine settings owner"), GEngine)) { return false; }
    TStrongObjectPtr<UGameUserSettings> PreviousSettings(GEngine->GameUserSettings);
    TStrongObjectPtr<UNarrativeGameUserSettings> HoldSettings(NewObject<UNarrativeGameUserSettings>());
    TGuardValue<TObjectPtr<UGameUserSettings>> ScopedSettings(GEngine->GameUserSettings, HoldSettings.Get());
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; }
    FText Error;
    if (!TestTrue(TEXT("Authored scene request is available"), F.Terminal->CanUse(F.Player, Error))) { AddError(Error.ToString()); return false; }
    TestFalse(TEXT("Scoped settings retain ordinary holding"), HoldSettings->UseTapInteractions());
    TestEqual(TEXT("Authored hold remains .35 seconds"), F.Terminal->Interactable->InteractionTime, .35f);
    F.Interaction->BeginInteract(); F.Interaction->TickComponent(.2f, LEVELTICK_All, nullptr); F.Interaction->EndInteract(); F.NextFrame();
    TestFalse(TEXT("Releasing early never queues a scene"), F.Terminal->IsRequestPending());
    auto* Observer = NewObject<USovCampaignTerminalTestObserver>(F.PC); F.PC->KeepAlive.Add(Observer);
    Observer->Callback = [&F]() { F.Terminal->Interactable->Deactivate(); };
    F.Interaction->FinishUseEvent().AddDynamic(Observer, &USovCampaignTerminalTestObserver::OnUse);
    F.Interaction->BeginInteract(); F.Interaction->TickComponent(.36f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("The full native hold queues its request"), F.Terminal->IsRequestPending());
    TestFalse(TEXT("Actual interaction completion callback deactivated the source"), F.Terminal->Interactable->IsActive());
    F.Interaction->EndInteract(); F.NextFrame();
    TestFalse(TEXT("Retired request releases its slot"), F.Terminal->IsRequestPending());
    TestEqual(TEXT("Retirement never starts the target scene"), F.Story->CampaignCinematic->GetPhase(), ESovCinematicPhase::Idle);
    TestEqual(TEXT("Input completion alone cannot supply cinematic proof"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    F.Interaction->FinishUseEvent().RemoveAll(Observer); F.Terminal->Interactable->Activate();
    TestTrue(TEXT("A new current request may queue"), F.Terminal->RequestUse(F.Player, Error)); F.NextFrame();
    // This fixture has no save subsystem. The real cinematic owner must reject its pre-scene
    // checkpoint and cannot turn an accepted interaction request into a fabricated viewing.
    TestEqual(TEXT("The scene's actual durability gate retains an empty journal"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    TestFalse(TEXT("Native target rejection leaves no pending input"), F.Terminal->IsRequestPending());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionRequestOwnershipTest, "ProjectVelkorran.Campaign.Aurelion.Request.QueuedReadinessTargetAndPossessionFences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionRequestOwnershipTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; } FText Error;
    TestTrue(TEXT("Initial scene request queues"), F.Terminal->RequestUse(F.Player, Error));
    F.ASC->SetCharacterReadyEpoch(F.ASC->GetCharacterReadyEpoch() + 1); F.NextFrame();
    TestEqual(TEXT("Retired readiness cannot start the scene"), F.Story->CampaignCinematic->GetPhase(), ESovCinematicPhase::Idle);
    TestTrue(TEXT("Current epoch can queue afresh"), F.Terminal->RequestUse(F.Player, Error));
    F.Terminal->Story = nullptr; F.NextFrame(); F.Terminal->Story = F.Story;
    TestEqual(TEXT("Detached authored target cannot start the scene"), F.Story->CampaignCinematic->GetPhase(), ESovCinematicPhase::Idle);
    TestTrue(TEXT("Restored target permits a new request"), F.Terminal->RequestUse(F.Player, Error));
    F.PC->UnPossess(); F.NextFrame();
    TestEqual(TEXT("Unpossession commits no campaign fact"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    TestFalse(TEXT("All ownership failures release the pending slot"), F.Terminal->IsRequestPending());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionRequestAdmissionTest, "ProjectVelkorran.Campaign.Aurelion.Request.PhysicalTypedTargetsAndMissionBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionRequestAdmissionTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; } FText Error;
    auto* Block = F.Blocker(); TestFalse(TEXT("Opaque geometry blocks the request"), F.Terminal->CanUse(F.Player, Error)); Block->Destroy();
    F.Terminal->DestinationMission = F.Mission;
    TestFalse(TEXT("Mixed scene and travel targets are rejected"), F.Terminal->CanUse(F.Player, Error)); F.Terminal->DestinationMission = nullptr;
    F.Terminal->BeatId = TEXT("Exit"); TestFalse(TEXT("Future objectives expose no actionable prompt"), F.Terminal->CanUse(F.Player, Error)); F.Terminal->BeatId = TEXT("Operate");
    F.Terminal->Operation = ESovAurelionRequest::TravelToMission; F.Terminal->Story = nullptr; F.Terminal->DestinationMission = F.Mission;
    TestFalse(TEXT("An incomplete mission cannot use its scene as a travel shortcut"), F.Terminal->RequestUse(F.Player, Error));
    F.Terminal->Operation = ESovAurelionRequest::CoAction; F.Terminal->DestinationMission = nullptr;
    TestFalse(TEXT("No companion receipt can be manufactured without its native target"), F.Terminal->RequestUse(F.Player, Error));
    TestEqual(TEXT("Rejected physical and typed requests preserve the journal"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionDialogueClockTest, "ProjectVelkorran.Campaign.Aurelion.Story.ReadableClockAndPersistedPrioritySelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionDialogueClockTest::RunTest(const FString& Parameters)
{
    FSovAurelionDialogueCue Common; Common.Speaker = FText::FromString(TEXT("Tarrik")); Common.Text = FText::FromString(TEXT("Both groups are clear.")); Common.DurationSeconds = 4.f;
    FSovAurelionDialogueCue West = Common; West.StartSeconds = 4.f; West.RequiredPriority = ESovAurelionRescuePriority::WestStretchers;
    West.Text = FText::FromString(TEXT("We protected the stretchers first."));
    FSovAurelionDialogueCue East = West; East.RequiredPriority = ESovAurelionRescuePriority::EastWalkers;
    East.Text = FText::FromString(TEXT("We protected the walkers first."));
    TArray<FSovAurelionDialogueCue> Cues = { Common, West, East }; FString Error;
    TestTrue(TEXT("Mutually exclusive aftermath lines share one finite clock interval"), ASovAurelionStorySequenceActor::ValidateDialogueCues(Cues, 8.0, Error));
    TestEqual(TEXT("Playback start selects the common line"), ASovAurelionStorySequenceActor::FindDialogueCue(Cues, 0), 0);
    TestEqual(TEXT("West journal choice selects only west acknowledgement"), ASovAurelionStorySequenceActor::FindDialogueCue(Cues, 4.0, ESovAurelionRescuePriority::WestStretchers), 1);
    TestEqual(TEXT("East journal choice selects only east acknowledgement"), ASovAurelionStorySequenceActor::FindDialogueCue(Cues, 4.0, ESovAurelionRescuePriority::EastWalkers), 2);
    TestEqual(TEXT("Unset priority invents neither aftermath"), ASovAurelionStorySequenceActor::FindDialogueCue(Cues, 4.0), INDEX_NONE);
    TestEqual(TEXT("Clip end has no active line"), ASovAurelionStorySequenceActor::FindDialogueCue(Cues, 8.0, ESovAurelionRescuePriority::WestStretchers), INDEX_NONE);
    Cues[1].StartSeconds = 3.f;
    TestFalse(TEXT("Overlapping common and branch speech is rejected"), ASovAurelionStorySequenceActor::ValidateDialogueCues(Cues, 8.0, Error));
    Cues[1].StartSeconds = 4.f; Cues[1].DurationSeconds = .1f;
    TestFalse(TEXT("Unreadable receipt-length dialogue is rejected"), ASovAurelionStorySequenceActor::ValidateDialogueCues(Cues, 8.0, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionThermalRequestReplacementTest,
    "ProjectVelkorran.Campaign.Aurelion.Request.ThermalReplacementRequiresFreshInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionThermalRequestReplacementTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed request player initialized"), F.ASC)) { return false; }
    FText Error; FString NativeError;
    auto* Source = F.World->SpawnActor<ASovAxiomRuntimeTestCharacter>(); Source->InitializeTestCombat(0);
    Source->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
    auto* Companion = F.World->SpawnActorDeferred<ASovAurelionThermalTestCompanion>(ASovAurelionThermalTestCompanion::StaticClass(),
        FTransform(FVector(0, 500, 0)), F.PC, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Separate native companion actor"), Companion)
        || !TestTrue(TEXT("Proxy copies a real matching source"), Companion->PrepareProxy(FSovGameplayTags::Get().Character_Player_Selene,
            TEXT("Selene"), Source->GetNarrativeAbilitySystemComponent(), {}, NativeError))) { return false; }
    Source->Destroy(); Companion->FinishSpawning(FTransform(FVector(0, 500, 0))); Companion->InitializeTestCombat();
    auto* ActiveProperty = FindFProperty<FObjectPropertyBase>(USovConvergenceCompanionState::StaticClass(), TEXT("Active"));
    if (!TestNotNull(TEXT("Existing campaign companion owner"), ActiveProperty)) { return false; }
    ActiveProperty->SetObjectPropertyValue_InContainer(F.PC->GetConvergenceCompanionState(), Companion);
    auto* Director = F.World->SpawnActor<ASovAurelionThermalTestDirector>(); Director->EncounterId = TEXT("Aurelion.Request.Retry");
    F.Mission->Beats[0].RequiredEncounterId = Director->EncounterId;
    F.Terminal->Story = nullptr; F.Terminal->Operation = ESovAurelionRequest::FrostSetup;
    F.Terminal->ThermalDirector = Director; F.Terminal->ThermalParticipantId = TEXT("E4.Elite");
    auto* Anchor = F.World->SpawnActor<AActor>(); Anchor->Tags.Add(TEXT("Test.Retry.CleanFrost"));
    const auto MakeElite = [&F, Director, Anchor](FVector Position, bool bAuthored)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor = F.World->SpawnActor<ASovCampaignMassRoundTripNPC>(ASovCampaignMassRoundTripNPC::StaticClass(), Position, FRotator::ZeroRotator, Spawn);
        Actor->InitializeTestCombat();
        auto* Component = NewObject<USovAurelionEliteThermalFracture>(Actor);
        Actor->AddInstanceComponent(Component);
        if (bAuthored)
        {
            Component->EncounterDirector = Director; Component->FrostAnchor = Anchor;
            Component->FrostAnchorId = TEXT("Test.Retry.CleanFrost");
        }
        Component->RegisterComponent(); Component->Activate(); return Component;
    };
    auto* Initial = MakeElite(FVector(650, 350, 0), true);
    if (!TestTrue(TEXT("Initial elite has exact director participant ownership"), Director->RegisterParticipant(TEXT("E4.Elite"), Cast<ASovNPCCharacterBase>(Initial->GetOwner()), true))) { return false; }
    Director->StartTestAttempt(F.Player);
    TestEqual(TEXT("Stable authored binding resolves its current elite"), F.Terminal->GetCurrentThermalTarget(), static_cast<USovAurelionThermalFractureComponent*>(Initial));
    if (!TestTrue(TEXT("A real request queues against the initial component"), F.Terminal->RequestUse(F.Player, Error))) { AddError(Error.ToString()); return false; }
    INarrativeSavableComponent::Execute_PrepareForSave(Initial);
    TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer, true);
        Archive.ArIsSaveGame = true; Archive.ArNoDelta = true; Initial->Serialize(Archive);
    }
    auto* Replacement = MakeElite(FVector(650, 350, 0), false);
    {
        FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Archive(Reader, true);
        Archive.ArIsSaveGame = true; Replacement->Serialize(Archive);
    }
    INarrativeSavableComponent::Execute_Load(Replacement);
    TestTrue(TEXT("Saved anchor record is accepted"), Replacement->WasSaveRecordLoadAccepted());
    TestEqual(TEXT("Saved anchor resolves without manual repair"), Replacement->FrostAnchor.Get(), Anchor);
    TestNull(TEXT("SaveGame does not retain the authored director"), Replacement->EncounterDirector.Get());
    TestFalse(TEXT("Unpublished replacement cannot bind early"), Replacement->InitializeBindings());
    TestNull(TEXT("Failed early binding does not publish an owner"), Replacement->EncounterDirector.Get());
    // Supply the same roster publication a native restore performs. The old actor
    // deliberately remains live, proving identity fencing independent of destruction.
    Director->Participants[0].Character = Cast<ASovNPCCharacterBase>(Replacement->GetOwner());
    TestTrue(TEXT("Published replacement recovers its validated director"), Replacement->InitializeBindings());
    TestEqual(TEXT("Runtime binding publishes the exact director"), Replacement->EncounterDirector.Get(), static_cast<ASovEncounterDirector*>(Director));
    for (const auto Operation : {ESovAurelionRequest::MoveFrostPartner, ESovAurelionRequest::FrostSetup, ESovAurelionRequest::HeatConfirm})
    {
        F.Terminal->Operation = Operation;
        TestEqual(TEXT("Every thermal control resolves the restored elite"), F.Terminal->GetCurrentThermalTarget(), static_cast<USovAurelionThermalFractureComponent*>(Replacement));
    }
    F.Terminal->Operation = ESovAurelionRequest::FrostSetup;
    TestEqual(TEXT("New input resolves the replacement even while the old component lives"), F.Terminal->GetCurrentThermalTarget(), static_cast<USovAurelionThermalFractureComponent*>(Replacement));
    F.NextFrame();
    TestFalse(TEXT("Retired queued input releases its slot"), F.Terminal->IsRequestPending());
    TestEqual(TEXT("Retired input reports ownership change rather than target acceptance"), F.Terminal->LastResult.ToString(), FString(TEXT("Interaction changed; try again")));
    TestEqual(TEXT("Replacement receives no old frost request"), Replacement->GetFractureWindowRemainingSeconds(), 0.f);
    TestTrue(TEXT("A fresh input can bind the replacement"), F.Terminal->RequestUse(F.Player, Error));
    Director->StartTestAttempt(F.Player); F.NextFrame();
    TestEqual(TEXT("A new attempt independently retires pending input on the same component"), F.Terminal->LastResult.ToString(), FString(TEXT("Interaction changed; try again")));
    F.Terminal->Thermal = Initial;
    TestNull(TEXT("Direct and stable bindings together are ambiguous"), F.Terminal->GetCurrentThermalTarget());
    TestFalse(TEXT("Ambiguous authored binding exposes no action"), F.Terminal->CanUse(F.Player, Error));
    F.Terminal->ThermalDirector = nullptr; F.Terminal->ThermalParticipantId = NAME_None;
    TestEqual(TEXT("Legacy explicit component binding remains supported"), F.Terminal->GetCurrentThermalTarget(), static_cast<USovAurelionThermalFractureComponent*>(Initial));
    Initial->Deactivate(); TestNull(TEXT("An explicitly deactivated component cannot be admitted"), F.Terminal->GetCurrentThermalTarget());
    F.Terminal->Thermal = nullptr; F.Terminal->ThermalDirector = Director; F.Terminal->ThermalParticipantId = TEXT("MissingElite");
    TestNull(TEXT("Missing exact participant never falls back to another elite"), F.Terminal->GetCurrentThermalTarget());
    auto* OtherDirector = F.World->SpawnActor<ASovAurelionThermalTestDirector>();
    Replacement->EncounterDirector = OtherDirector;
    TestFalse(TEXT("Wrong explicit director is never repaired silently"), Replacement->InitializeBindings());
    TestEqual(TEXT("Wrong explicit constraint remains intact"), Replacement->EncounterDirector.Get(), static_cast<ASovEncounterDirector*>(OtherDirector));
    Replacement->EncounterDirector = nullptr;
    OtherDirector->Participants = Director->Participants;
    TestFalse(TEXT("Duplicate registration cannot choose an arbitrary director"), Replacement->InitializeBindings());
    TestNull(TEXT("Ambiguous ownership publishes no director"), Replacement->EncounterDirector.Get());
    OtherDirector->Participants.Empty();
    TestTrue(TEXT("Binding recovers when ownership becomes unique"), Replacement->InitializeBindings());
    TestEqual(TEXT("Existing bound ASC still republishes the validated reference"), Replacement->EncounterDirector.Get(), static_cast<ASovEncounterDirector*>(Director));
    TestFalse(TEXT("Restoring bindings never fabricates fracture proof"), Replacement->GetFractureReceipt().IsComplete());
    TestEqual(TEXT("Request retirement fabricates no campaign journal facts"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionOrdinaryHoldTest,
    "ProjectVelkorran.Campaign.Aurelion.Request.OrdinaryHoldMarkNeverSuppliesCoActionProof",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionOrdinaryHoldTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Ready managed player"), F.ASC)) { return false; }
    FString Reason; F.Mission->AllowedCompanionIds.Add(TEXT("Selene"));
    auto* NPC = F.World->SpawnActor<ASovCoActionTestNPC>(); NPC->InitializeTestCombat(0);
    auto* AI = F.World->SpawnActor<ASovCoActionTestNPCController>(); AI->Possess(NPC);
    CastChecked<USovCoActionTestActivities>(AI->GetActivityComponent())->InitializeForCoAction();
    auto* Component = NewObject<USovCompanionComponent>(NPC); NPC->AddInstanceComponent(Component); Component->RegisterComponent();
    Component->CompanionId = TEXT("Selene");
    if (!TestTrue(TEXT("Existing Narrative activity owns permitted follow command"), Component->SetLeader(F.Player, Reason))) { AddError(Reason); return false; }
    auto* Mark = F.World->SpawnActor<AActor>(); auto* Root = NewObject<USceneComponent>(Mark);
    Mark->AddInstanceComponent(Root); Mark->SetRootComponent(Root); Root->RegisterComponent(); Mark->SetActorLocation(FVector(700, 400, 0));
    const FVector Before = NPC->GetActorLocation();
    TestFalse(TEXT("An ordinary mark is not a campaign co-action anchor"), Component->CanRequestCommand(F.Player, ESovCompanionCommand::MoveToAnchor, Mark, Reason));
    if (!TestTrue(TEXT("Ordinary hold can request that physical mark"), Component->RequestCommand(F.Player, ESovCompanionCommand::HoldPosition, Mark, Reason))) { AddError(Reason); return false; }
    auto* Goal = Cast<USovCompanionCommandGoal>(AI->GetActivityComponent()->GetCurrentActivityGoal());
    if (!TestNotNull(TEXT("Actual existing Narrative slot owns the hold goal"), Goal)) { return false; }
    TestTrue(TEXT("Hold goal captures the authored mark instead of the player location"), Goal->bExplicitHoldTarget && Goal->Target == Mark && Goal->HoldLocation.Equals(Mark->GetActorLocation()));
    TestTrue(TEXT("Issuing a command never teleports its companion"), NPC->GetActorLocation().Equals(Before));
    TestEqual(TEXT("Ordinary movement supplies no campaign fact"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    Mark->Destroy(); TestFalse(TEXT("Retired mark invalidates its exact command"), Component->IsCommandCurrent(Goal));
    Component->TickContextCommand(Goal);
    TestTrue(TEXT("Retirement leaves room for a new native command"), Component->RequestCommand(F.Player, ESovCompanionCommand::HoldPosition, NPC, Reason));
    Goal = Cast<USovCompanionCommandGoal>(AI->GetActivityComponent()->GetCurrentActivityGoal());
    TestTrue(TEXT("Final departure self-hold preserves the actual companion exit"), Goal && Goal->bExplicitHoldTarget && Goal->Target == NPC && Goal->HoldLocation.Equals(NPC->GetActorLocation()));
    TestTrue(TEXT("Legacy hold without a target remains available"), Component->RequestCommand(F.Player, ESovCompanionCommand::HoldPosition, nullptr, Reason));
    Goal = Cast<USovCompanionCommandGoal>(AI->GetActivityComponent()->GetCurrentActivityGoal());
    TestTrue(TEXT("Legacy hold retains player-position semantics"), Goal && !Goal->bExplicitHoldTarget && Goal->HoldLocation.Equals(F.Player->GetActorLocation()));
    TestEqual(TEXT("Neither arrival intent nor departure intent awards proof"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionDefenseCadenceTest,
    "ProjectVelkorran.Campaign.Companion.DefenseDoesNotStarveOrdinaryAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionDefenseCadenceTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Ready managed player"), F.ASC)) { return false; }
    F.Mission->AllowedCompanionIds.Add(TEXT("Tarrik"));
    auto* NPC = F.World->SpawnActor<ASovCompanionCommandTestProxy>(); NPC->InitializeCommandCombat();
    NPC->SetActorLocation(F.Player->GetActorLocation() + FVector(0, -500, 0));
    auto* AI = F.World->SpawnActor<ASovCoActionTestNPCController>(); AI->Possess(NPC);
    CastChecked<USovCoActionTestActivities>(AI->GetActivityComponent())->InitializeForCoAction();
    auto* Target = F.World->SpawnActor<ASovCoActionTestNPC>(); Target->InitializeTestCombat(1);
    Target->SetActorLocation(NPC->GetActorLocation() + FVector(300, 0, 0));
    Target->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_NPC_Activity_Attacking);
    auto* Component = NPC->GetCompanionComponent(); Component->CompanionId = TEXT("Tarrik");
    Component->CuratedAbilities = {USovCompanionCommandTestDefense::StaticClass(), USovBotTestAttackAlpha::StaticClass()};
    auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
    const auto DefenseHandle = ASC->GiveAbility(FGameplayAbilitySpec(USovCompanionCommandTestDefense::StaticClass(), 1));
    const auto AttackHandle = ASC->GiveAbility(FGameplayAbilitySpec(USovBotTestAttackAlpha::StaticClass(), 1));
    auto* Defense = CastChecked<USovCompanionCommandTestDefense>(ASC->FindAbilitySpecFromHandle(DefenseHandle)->GetPrimaryInstance());
    auto* Attack = CastChecked<USovBotTestAttackAlpha>(ASC->FindAbilitySpecFromHandle(AttackHandle)->GetPrimaryInstance());
    FString Reason;
    if (!TestTrue(TEXT("Native companion command accepted"), Component->SetLeader(F.Player, Reason))) { AddError(Reason); return false; }
    // Publish a resolved player hit through the same delegate used by combat.
    // No production health or mission state is changed by the scheduler fixture.
    FSovDamageResult Hit; Hit.SourceActor = F.Player; Hit.TargetActor = Target; Hit.AppliedHealthDamage = 40.f;
    F.ASC->DamageResolvedAsSource(Hit);
    float Shield = 0.f, Health = 10.f, Poise = 0.f;
    TestTrue(TEXT("Resolved player hit supplies the native contribution budget"),
        Component->LimitSovDamage(Target, FGameplayEffectContextHandle(), Shield, Health, Poise));
    FNarrativeBotAttackCandidate Candidate;
    TestTrue(TEXT("Ordinary attack is initially eligible"), ASC->SelectBotAttack(Target, FGameplayTag(), Candidate));
    auto* Goal = Cast<USovCompanionCommandGoal>(AI->GetActivityComponent()->GetCurrentActivityGoal());
    if (!TestNotNull(TEXT("Native activity owns command"), Goal)) { return false; }
    Component->TickContextCommand(Goal);
    TestEqual(TEXT("Threat first triggers curated defense"), Defense->ActivationCount, 1);
    TestFalse(TEXT("Defense completed synchronously"), ASC->FindAbilitySpecFromHandle(DefenseHandle)->IsActive());
    TestTrue(TEXT("Completed defense retains ordinary attack eligibility"), ASC->SelectBotAttack(Target, FGameplayTag(), Candidate));
    Component->TickContextCommand(Goal);
    TestEqual(TEXT("Sustained enemy attack cannot repeat defense during its cadence"), Defense->ActivationCount, 1);
    TestEqual(TEXT("Completed defense leaves ordinary attack available immediately"), Attack->ActivationCount, 1);
    TestEqual(TEXT("A command attack exposes its actual target without a legacy Goal_Attack"),
        USovCompanionComponent::ResolveCommandAttackTarget(NPC), static_cast<ANarrativeCharacter*>(Target));
    AI->SetFocus(F.Player);
    TestNull(TEXT("Changed controller focus cannot retarget the owned swing"), USovCompanionComponent::ResolveCommandAttackTarget(NPC));
    AI->SetFocus(Target);
    TestEqual(TEXT("Restored exact attack focus remains valid"),
        USovCompanionComponent::ResolveCommandAttackTarget(NPC), static_cast<ANarrativeCharacter*>(Target));
    Attack->FinishTestAttack();
    TestNull(TEXT("Completed attack leaves no stale melee target"), USovCompanionComponent::ResolveCommandAttackTarget(NPC));
    Component->TickContextCommand(Goal);
    TestEqual(TEXT("Ordinary attack still respects its own cadence"), Attack->ActivationCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionPendingHoldTest,
    "ProjectVelkorran.Campaign.Aurelion.Request.AcceptedHoldSurvivesSuspendedActivitySelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionPendingHoldTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Ready managed player"), F.ASC)) { return false; }
    FString Reason; F.Mission->AllowedCompanionIds.Add(TEXT("Selene"));
    auto* NPC = F.World->SpawnActor<ASovCoActionTestNPC>(); NPC->InitializeTestCombat(0);
    auto* AI = F.World->SpawnActor<ASovCoActionTestNPCController>(); AI->Possess(NPC);
    auto* Activities = CastChecked<USovCoActionTestActivities>(AI->GetActivityComponent());
    Activities->InitializeForCoAction();
    auto* Component = NewObject<USovCompanionComponent>(NPC); NPC->AddInstanceComponent(Component); Component->RegisterComponent();
    Component->CompanionId = TEXT("Selene");
    if (!TestTrue(TEXT("Normal permitted leader initialized"), Component->SetLeader(F.Player, Reason))) { AddError(Reason); return false; }
    Activities->Deactivate();
    TestNull(TEXT("Real activity deactivation releases current selection"), Activities->GetCurrentActivityGoal());
    if (!TestTrue(TEXT("Suspended activity can retain an accepted ordinary self-hold"),
        Component->RequestCommand(F.Player, ESovCompanionCommand::HoldPosition, NPC, Reason))) { AddError(Reason); return false; }
    bool bFound = false;
    auto* Accepted = Cast<USovCompanionCommandGoal>(Activities->GetGoalByKey(USovCompanionCommandGoal::StaticClass(), Component, bFound));
    if (!TestTrue(TEXT("The real Narrative goal registry accepted this exact hold"), bFound && Accepted && Component->IsCommandCurrent(Accepted))) { return false; }
    TestNull(TEXT("Acceptance does not activate a suspended activity"), Activities->GetCurrentActivityGoal());
    TestTrue(TEXT("Departure query recognizes accepted intent before selection"), Component->HasAcceptedHoldPosition(NPC));
    const FGuid AcceptedId = Accepted->RequestId;
    for (int32 Poll = 0; Poll < 30; ++Poll)
    {
        if (!Component->HasAcceptedHoldPosition(NPC))
        { Component->RequestCommand(F.Player, ESovCompanionCommand::HoldPosition, NPC, Reason); }
    }
    TestEqual(TEXT("Repeated presentation reconciliation retains the original request identity"),
        Activities->GetGoalByKey(USovCompanionCommandGoal::StaticClass(), Component, bFound), static_cast<UNPCGoalItem*>(Accepted));
    TestEqual(TEXT("The pending request was never replaced"), Accepted->RequestId, AcceptedId);
    Activities->Activate();
    TestEqual(TEXT("Resumed activity selects the same accepted goal"), Activities->GetCurrentActivityGoal(), static_cast<UNPCGoalItem*>(Accepted));
    TestTrue(TEXT("Selected hold still satisfies the same query"), Component->HasAcceptedHoldPosition(NPC));
    Component->CancelContextCommand();
    TestFalse(TEXT("Explicit cancellation removes accepted intent"), Component->HasAcceptedHoldPosition(NPC));
    TestEqual(TEXT("Pending and selected holds never award campaign proof"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionRequestFocusPriorityTest,
    "ProjectVelkorran.Campaign.Aurelion.Request.RefusedPromptCannotTakeFocusFromAUsableControl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionRequestFocusPriorityTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Ready managed player"), F.ASC)) { return false; }
    // A refusing prompt stands nearer than the control, as the living Elite did beside E4B's FrostSetup.
    auto* Hostile = F.World->SpawnActor<AActor>();
    auto* Body = NewObject<UBoxComponent>(Hostile); Hostile->SetRootComponent(Body); Hostile->AddInstanceComponent(Body);
    Body->SetBoxExtent(FVector(34, 34, 88)); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Body->RegisterComponent();
    Hostile->SetActorLocation(F.Player->GetActorLocation() + FVector(100, 0, 0));
    auto* HostilePrompt = NewObject<USovRefusingTestInteractable>(Hostile);
    Hostile->AddInstanceComponent(HostilePrompt); HostilePrompt->RegisterComponent(); HostilePrompt->Activate();
    auto* Interaction = NewObject<USovAurelionFocusTestInteraction>(F.PC);
    F.PC->AddInstanceComponent(Interaction); Interaction->RegisterComponent(); Interaction->Configure(F.PC);
    // This fixture world never begins play, so both candidates enter the same public registry the game reads.
    auto* Registry = F.World->GetSubsystem<UInteractionSubsystem>();
    if (!TestNotNull(TEXT("The world supplies the native interaction registry"), Registry)) { return false; }
    F.Terminal->Interactable->Activate(); HostilePrompt->Activate();
    Registry->CacheInteractable(F.Terminal->Interactable); Registry->CacheInteractable(HostilePrompt);
    if (!TestTrue(TEXT("The required control is within native reach"),
            Interaction->IsInteractableInReach(F.Terminal->Interactable))
        || !TestTrue(TEXT("The nearer hostile prompt is within native reach"),
            Interaction->IsInteractableInReach(HostilePrompt)))
    { return false; }
    TestEqual(TEXT("Neither prompt is given an authored priority advantage"),
        F.Terminal->Interactable->InteractionPriority, HostilePrompt->InteractionPriority);
    Interaction->PerformInteractionCheck(0.f);
    TestEqual(TEXT("A nearer refused prompt cannot take focus from the usable control"),
        Interaction->Viewed(), static_cast<const UNarrativeInteractableComponent*>(F.Terminal->Interactable));
    // The same nearer prompt wins once it admits interaction, so admission is what decided the contest.
    HostilePrompt->bAdmit = true;
    Interaction->PerformInteractionCheck(0.f);
    TestEqual(TEXT("Range still decides between two usable prompts"),
        Interaction->Viewed(), static_cast<const UNarrativeInteractableComponent*>(HostilePrompt));
    // With nothing usable in reach the refusal is still shown, so its own error text can explain itself.
    HostilePrompt->bAdmit = false; F.Terminal->Interactable->Deactivate();
    Interaction->PerformInteractionCheck(0.f);
    TestEqual(TEXT("A refusal is still focused when no usable prompt is in reach"),
        Interaction->Viewed(), static_cast<const UNarrativeInteractableComponent*>(HostilePrompt));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionContributionWindowTest,
	"ProjectVelkorran.Campaign.Companion.ContributionOpensBeforeThePlayersFirstHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCompanionContributionWindowTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	// Companion contribution is budgeted from the player's damage, so before the player's first hit the
	// budget was zero and the companion attacked nothing - every encounter opening, and any stretch the
	// player spent guarding or evading. §12.2 asks it to contribute visibly (audit EA2-05).
	using namespace SovResonancePolicy;
	const float Fraction = .2f;

	// Without an allowance the old behaviour stands: no player damage, no budget, no attack.
	TestFalse(TEXT("A zero-length allowance leaves the budget in charge"), WithinOpeningContribution(10., 10., 0.f));
	TestFalse(TEXT("With no player damage and no allowance the companion may not commit"),
		MayCommitAttack(false, 0.f, 0.f, Fraction, 5.f));

	// Inside the allowance it fights regardless of the budget.
	TestTrue(TEXT("The allowance is open at the moment the scope opens"), WithinOpeningContribution(100., 100., 8.f));
	TestTrue(TEXT("The allowance is still open just before it lapses"), WithinOpeningContribution(107.9, 100., 8.f));
	TestFalse(TEXT("The allowance closes on its own"), WithinOpeningContribution(108.1, 100., 8.f));
	TestTrue(TEXT("Inside the allowance the companion may commit with no player damage"),
		MayCommitAttack(true, 0.f, 0.f, Fraction, 5.f));

	// Once it closes, the ordinary cap resumes - and the damage dealt during it still counts, so a
	// companion that spent the opening freely waits for the player to catch up.
	TestFalse(TEXT("After the allowance a spent companion waits for the player"),
		MayCommitAttack(false, 40.f, 0.f, Fraction, 5.f));
	TestTrue(TEXT("Player damage reopens the budget"), MayCommitAttack(false, 0.f, 100.f, Fraction, 5.f));

	// Refuse rather than clamp: a budget too small to matter declines the attack instead of spending it
	// on a swing that animates in full and lands for nothing.
	const float Remaining = RemainingContribution(24.f, 100.f, Fraction);
	TestTrue(TEXT("The remaining budget is genuinely small"), Remaining > 0.f && Remaining < 5.f);
	TestFalse(TEXT("A budget below a meaningful amount refuses the attack"),
		MayCommitAttack(false, 24.f, 100.f, Fraction, 5.f));
	TestTrue(TEXT("The same budget is accepted when any amount counts"),
		MayCommitAttack(false, 24.f, 100.f, Fraction, 0.f));

	// Degenerate inputs fail closed rather than handing out an unbudgeted attack.
	TestFalse(TEXT("A non-finite minimum refuses"),
		MayCommitAttack(false, 0.f, 100.f, Fraction, std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("A non-finite clock is not inside any allowance"),
		WithinOpeningContribution(std::numeric_limits<double>::quiet_NaN(), 100., 8.f));
	TestFalse(TEXT("A clock behind the scope start is not inside the allowance"),
		WithinOpeningContribution(99., 100., 8.f));
	return true;
}
#endif
