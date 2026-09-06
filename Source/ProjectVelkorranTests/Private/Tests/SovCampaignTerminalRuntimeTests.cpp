// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCampaignTerminalRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Campaign/SovCampaignInteractionTerminal.h"
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
        ASovCampaignInteractionTerminal* Terminal = nullptr;
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
            Mission->MissionId = TEXT("M12_TerminalAutomation"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
            Mission->PawnClass = ASovHandoffRuntimeTestPawn::StaticClass(); Mission->PlayerDefinition = Definition;
            FSovCampaignBeatDefinition Operate; Operate.BeatId = TEXT("Operate"); Operate.RequiredProtagonist = Mission->Protagonist;
            Operate.ObjectiveText = FText::FromString(TEXT("Operate the test terminal"));
            FSovCampaignBeatDefinition Exit; Exit.BeatId = TEXT("Exit"); Exit.PrerequisiteBeats = { Operate.BeatId };
            Exit.ObjectiveText = FText::FromString(TEXT("Reach the exit"));
            Mission->Beats = { Operate, Exit };
            if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { ASC = nullptr; return; }
            Terminal = World->SpawnActor<ASovCampaignInteractionTerminal>();
            Terminal->TerminalId = TEXT("TerminalA"); Terminal->MissionId = Mission->MissionId; Terminal->CompletionBeat = Operate.BeatId;
            Terminal->bWriteCheckpoint = false;
            Terminal->SetActorLocation(Player->GetActorLocation() + FVector(180, 0, 0));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTerminalNativeInputTest, "ProjectVelkorran.Campaign.Terminal.NativeInteractionCommitsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTerminalNativeInputTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; }
    FText Error;
    TestTrue(TEXT("Terminal actor follows normal initialized lifecycle"), F.Terminal->IsActorInitialized());
    TestTrue(TEXT("Native interactable registered"), F.Terminal->Interactable->IsRegistered());
    TestTrue(TEXT("Native auto-activation occurred"), F.Terminal->Interactable->IsActive());
    TestEqual(TEXT("Native interaction range is preserved"), F.Terminal->Interactable->InteractionDistance, 300.f);
    TestTrue(TEXT("Physical placement is within authored reach"), FVector::Dist(F.Player->GetActorLocation(), F.Terminal->GetActorLocation()) < 300.f);
    if (!TestTrue(TEXT("Physical terminal is available"), F.Terminal->CanUse(F.Player, Error))) { AddError(Error.ToString()); return false; }
    // Tap mode is a supported native interaction path; focus is the only supplied external input.
    auto* Observer = NewObject<USovCampaignTerminalTestObserver>(F.PC); F.PC->KeepAlive.Add(Observer);
    auto* EventProperty = FindFProperty<FMulticastDelegateProperty>(UNarrativeInteractableComponent::StaticClass(), TEXT("OnInteracted"));
    if (!TestNotNull(TEXT("Actual authored interactable delegate is reflected"), EventProperty)) { return false; }
    auto* Event = EventProperty->ContainerPtrToValuePtr<FOnInteract>(F.Terminal->Interactable);
    Event->AddDynamic(Observer, &USovCampaignTerminalTestObserver::OnInteracted);
    F.Terminal->Interactable->InteractionTime = 0.f;
    F.Interaction->BeginInteract(); F.Interaction->EndInteract();
    TestTrue(TEXT("Native BeginInteract dispatched a terminal request"), F.Terminal->IsRequestPending());
    TestEqual(TEXT("Ordinary per-interactable listener still fires once"), Observer->InteractedCount, 1);
    TestEqual(TEXT("No mission callback runs inside input dispatch"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    TestFalse(TEXT("Second request cannot queue behind the same input"), F.Terminal->RequestUse(F.Player, Error));
    F.NextFrame();
    TestTrue(TEXT("Ordinary beat is committed"), F.PC->GetCampaignState()->IsBeatComplete(F.Mission->MissionId, TEXT("Operate")));
    TestTrue(TEXT("Next immediate objective becomes actionable"), F.PC->GetCampaignState()->GetActionableObjectiveIds() == TArray<FName>{ TEXT("Exit") });
    TestEqual(TEXT("Exactly one native fact is written"), F.PC->GetCampaignState()->GetJournal().Num(), 1);
    TestFalse(TEXT("Finished non-checkpoint terminal cannot replay"), F.Terminal->RequestUse(F.Player, Error));
    F.NextFrame(); TestEqual(TEXT("Repeated input grants no duplicate fact"), F.PC->GetCampaignState()->GetJournal().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTerminalPhysicalAdmissionTest, "ProjectVelkorran.Campaign.Terminal.PhysicalAndSpecialProofAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTerminalPhysicalAdmissionTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; } FText Error;
    auto* Block = F.Blocker(); TestFalse(TEXT("Opaque world geometry blocks completion"), F.Terminal->RequestUse(F.Player, Error)); Block->Destroy();
    const FVector Position = F.Terminal->GetActorLocation(); F.Terminal->SetActorLocation(Position + FVector(1000, 0, 0));
    TestFalse(TEXT("Remote caller cannot complete an out-of-range terminal"), F.Terminal->RequestUse(F.Player, Error)); F.Terminal->SetActorLocation(Position);
    F.Mission->Beats[0].RequiredProtagonist = FSovGameplayTags::Get().Character_Player_Selene;
    TestFalse(TEXT("Wrong lead cannot commit"), F.Terminal->RequestUse(F.Player, Error)); F.Mission->Beats[0].RequiredProtagonist = F.Mission->Protagonist;
    F.Mission->Beats[0].bCanonGate = true; TestFalse(TEXT("Generic terminal never manufactures a canon gate"), F.Terminal->RequestUse(F.Player, Error)); F.Mission->Beats[0].bCanonGate = false;
    F.Mission->Beats[0].bRequiresCoActionProof = true; TestFalse(TEXT("Co-action proof cannot be forged"), F.Terminal->RequestUse(F.Player, Error)); F.Mission->Beats[0].bRequiresCoActionProof = false;
    F.Mission->Beats[0].CinematicId = TEXT("NeverPlayed"); TestFalse(TEXT("Cinematic viewing cannot be forged"), F.Terminal->RequestUse(F.Player, Error)); F.Mission->Beats[0].CinematicId = NAME_None;
    F.Terminal->CompletionBeat = TEXT("Exit"); TestFalse(TEXT("Unavailable future beat cannot be committed"), F.Terminal->RequestUse(F.Player, Error));
    TestEqual(TEXT("All rejected routes leave the durable journal untouched"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTerminalQueuedOwnershipTest, "ProjectVelkorran.Campaign.Terminal.QueuedReadinessAndPossessionRetirement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTerminalQueuedOwnershipTest::RunTest(const FString& Parameters)
{
    {
        FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; } FText Error;
        TestTrue(TEXT("Ready pawn may request"), F.Terminal->RequestUse(F.Player, Error));
        F.Player->SetTestVisualReady(false);
        TestFalse(TEXT("Visual retirement actually makes the pawn not ready"), F.Player->IsCharacterReady());
        F.NextFrame(); TestEqual(TEXT("Not-ready pawn cannot authorize the queued request"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
        TestFalse(TEXT("Retired request releases pending state"), F.Terminal->IsRequestPending());
        F.Player->SetTestVisualReady(true);
        TestTrue(TEXT("Visual readiness may recover without claiming a new authority epoch"), F.Player->IsCharacterReady());
        TestTrue(TEXT("Recovered pawn may make a fresh request"), F.Terminal->RequestUse(F.Player, Error));
        const int32 Epoch = F.ASC->GetCharacterReadyEpoch();
        F.ASC->SetCharacterReadyEpoch(Epoch + 1);
        TestEqual(TEXT("Authority setter actually publishes the newer epoch"), F.ASC->GetCharacterReadyEpoch(), Epoch + 1);
        F.NextFrame(); TestEqual(TEXT("An older authority epoch cannot authorize queued work"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    }
    {
        FTerminalWorld F; if (!TestNotNull(TEXT("Second player initialized"), F.ASC)) { return false; } FText Error;
        TestTrue(TEXT("Possessed pawn may request"), F.Terminal->RequestUse(F.Player, Error)); F.PC->UnPossess(); F.NextFrame();
        TestEqual(TEXT("Possession loss before deferred work commits nothing"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
        TestFalse(TEXT("Possession loss leaves no queued work"), F.Terminal->IsRequestPending());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTerminalCheckpointRetryTest, "ProjectVelkorran.Campaign.Terminal.CheckpointUnavailableRetryPreservesJournal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTerminalCheckpointRetryTest::RunTest(const FString& Parameters)
{
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; } FText Error;
    // This world intentionally has no GameInstance/slot owner. No real disk write is attempted.
    F.Terminal->bWriteCheckpoint = true;
    TestTrue(TEXT("First objective operation queues"), F.Terminal->RequestUse(F.Player, Error)); F.NextFrame();
    TestEqual(TEXT("Objective survives checkpoint admission failure"), F.PC->GetCampaignState()->GetJournal().Num(), 1);
    TestTrue(TEXT("Failure is reported, never called saved"), F.Terminal->LastResult.ToString().Contains(TEXT("Checkpoint unavailable")));
    TestTrue(TEXT("Player can retry the failed boundary"), F.Terminal->RequestUse(F.Player, Error)); F.NextFrame();
    TestEqual(TEXT("Retry does not duplicate the prior fact"), F.PC->GetCampaignState()->GetJournal().Num(), 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTerminalNativeHoldTest, "ProjectVelkorran.Campaign.Terminal.NativeHoldAndCompletionCallbackRetirement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTerminalNativeHoldTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine settings owner"), GEngine)) { return false; }
    TStrongObjectPtr<UGameUserSettings> PreviousSettings(GEngine->GameUserSettings);
    TStrongObjectPtr<UNarrativeGameUserSettings> HoldSettings(NewObject<UNarrativeGameUserSettings>());
    TGuardValue<TObjectPtr<UGameUserSettings>> ScopedSettings(GEngine->GameUserSettings, HoldSettings.Get());
    FTerminalWorld F; if (!TestNotNull(TEXT("Managed player initialized"), F.ASC)) { return false; }
    TestFalse(TEXT("The scoped settings use ordinary holding"), UNarrativeGameUserSettings::GetSovSettings()->UseTapInteractions());
    TestEqual(TEXT("Authored hold duration is unchanged"), F.Terminal->Interactable->InteractionTime, .35f);
    F.Interaction->BeginInteract(); F.Interaction->TickComponent(.2f, LEVELTICK_All, nullptr); F.Interaction->EndInteract(); F.NextFrame();
    TestFalse(TEXT("Early release never queues"), F.Terminal->IsRequestPending());
    TestEqual(TEXT("Early release writes no fact"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    auto* Observer = NewObject<USovCampaignTerminalTestObserver>(F.PC); F.PC->KeepAlive.Add(Observer);
    Observer->Callback = [&F]() { F.Terminal->Interactable->Deactivate(); };
    F.Interaction->FinishUseEvent().AddDynamic(Observer, &USovCampaignTerminalTestObserver::OnUse);
    F.Interaction->BeginInteract(); F.Interaction->TickComponent(.2f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Incomplete hold still waits"), F.Terminal->IsRequestPending());
    F.Interaction->TickComponent(.16f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Crossing the normal hold threshold queues exactly one request"), F.Terminal->IsRequestPending());
    TestFalse(TEXT("Real completion callback retired its interactable"), F.Terminal->Interactable->IsActive());
    F.Interaction->EndInteract(); F.NextFrame();
    TestEqual(TEXT("Retired interactable cannot publish any journal fact"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    TestFalse(TEXT("Cancelled request releases its pending slot"), F.Terminal->IsRequestPending());
    F.Interaction->FinishUseEvent().RemoveAll(Observer); F.Terminal->Interactable->Activate();
    F.Interaction->BeginInteract(); F.Interaction->TickComponent(.36f, LEVELTICK_All, nullptr);
    F.Interaction->TickComponent(.5f, LEVELTICK_All, nullptr); F.Interaction->EndInteract(); F.NextFrame();
    TestEqual(TEXT("A fresh uninterrupted hold commits exactly once"), F.PC->GetCampaignState()->GetJournal().Num(), 1);
    return true;
}
#endif
