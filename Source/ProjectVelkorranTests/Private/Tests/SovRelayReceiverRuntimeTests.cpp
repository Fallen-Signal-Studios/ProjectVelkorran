// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignRelayReceiver.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Tests/SovCampaignTerminalRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "AI/NPCDefinition.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace SovRelayTests
{
    struct FWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovHandoffRuntimeTestController* PC = nullptr;
        ASovHandoffRuntimeTestPawn* Player = nullptr;
        UNarrativeAbilitySystemComponent* ASC = nullptr;
        USovCampaignDefinition* Mission = nullptr;
        ASovEncounterDirector* Director = nullptr;
        ASovCampaignEncounterObjective* Objective = nullptr;
        ASovCampaignRelayReceiver* Receivers[2] = {};
        USovCampaignTerminalTestInteraction* Interaction = nullptr;
        FString Error;
        uint64 Frame = GFrameCounter;
        FWorld()
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeActorsForPlay(FURL()); World->GetTimerManager().Tick(0.f);
            PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
            Player = World->SpawnActor<ASovHandoffRuntimeTestPawn>(); auto* PS = World->SpawnActor<ASovPlayerState>();
            if (!PC || !Player || !PS) { return; }
            World->AddController(PC);
            auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            Player->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Player);
            if (!Player->StageTestReadiness(PS, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
            ASC = Player->GetNarrativeAbilitySystemComponent();
            Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
            Mission->MissionId = TEXT("RelayReceiverTest"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
            Mission->PawnClass = ASovHandoffRuntimeTestPawn::StaticClass(); Mission->PlayerDefinition = Definition;
            FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("RelayOverlook"); Beat.RequiredProtagonist = Mission->Protagonist;
            Beat.RequiredEncounterId = TEXT("Test.RelayOverlook"); Beat.RequiredReceiverIds = {TEXT("West"), TEXT("East")};
            Beat.ObjectiveText = FText::FromString(TEXT("Defeat the formation and disable both receivers"));
            FSovCampaignBeatDefinition Exit; Exit.BeatId = TEXT("Exit"); Exit.PrerequisiteBeats = { Beat.BeatId };
            Exit.ObjectiveText = FText::FromString(TEXT("Leave the overlook")); Mission->Beats = {Beat, Exit};
            if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { ASC = nullptr; return; }
            Director = World->SpawnActor<ASovEncounterDirector>(); Director->EncounterId = Beat.RequiredEncounterId;
            auto* NPCDefinition = NewObject<UNPCDefinition>(PC); PC->KeepAlive.Add(NPCDefinition);
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* NPC = World->SpawnActor<ASovCampaignMassRoundTripNPC>(ASovCampaignMassRoundTripNPC::StaticClass(), FVector(1500, 0, 0), FRotator::ZeroRotator, Spawn);
            if (!NPC) { ASC = nullptr; return; }
            NPC->SetNPCDefinition(NPCDefinition); auto* NPCASC = NPC->GetNarrativeAbilitySystemComponent();
#define SOV_RELAY_RESOURCE(Name) NPCASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMax##Name##Attribute(), 100.f); NPCASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), 100.f);
            SOV_RELAY_RESOURCE(Health) SOV_RELAY_RESOURCE(Shield) SOV_RELAY_RESOURCE(Stamina) SOV_RELAY_RESOURCE(Poise) SOV_RELAY_RESOURCE(Echo)
#undef SOV_RELAY_RESOURCE
            if (!Director->RegisterParticipant(TEXT("Guard"), NPC, true)) { ASC = nullptr; return; }
            Objective = World->SpawnActor<ASovCampaignEncounterObjective>(); Objective->EncounterDirector = Director;
            Objective->MissionId = Mission->MissionId; Objective->CompletionBeat = Beat.BeatId;
            for (int32 I = 0; I < 2; ++I)
            {
                Receivers[I] = World->SpawnActor<ASovCampaignRelayReceiver>();
                Receivers[I]->ReceiverId = I == 0 ? FName(TEXT("West")) : FName(TEXT("East"));
                Receivers[I]->EncounterObjective = Objective; Receivers[I]->SetActorLocation(FVector(300 + I * 400, I * 300, 0));
                Objective->RequiredReceivers.Add(Receivers[I]);
            }
            Interaction = NewObject<USovCampaignTerminalTestInteraction>(PC); PC->AddInstanceComponent(Interaction);
            Interaction->RegisterComponent(); Interaction->Configure(PC); Interaction->Activate();
            auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
            if (!Save || !Save->UpdateSaveObject(true)) { ASC = nullptr; }
        }
        ~FWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        bool Start() { return Objective && Objective->StartEncounter(Player, Error); }
        void FrameOnce() { TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame); World->GetTimerManager().Tick(.016f); }
        void KillGuard()
        {
            auto* NPC = Director->GetParticipant(TEXT("Guard"));
            auto* EnemyASC = CastChecked<USovCoordinationTestASC>(NPC->GetNarrativeAbilitySystemComponent());
            EnemyASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
            EnemyASC->SeedDead(true); EnemyASC->OnDeathStateChanged.Broadcast(NPC, EnemyASC, true);
        }
        void Approach(int32 Index) { Player->SetActorLocation(Receivers[Index]->GetActorLocation() - FVector(180, 0, 0)); }
        void Operate(int32 Index)
        {
            Approach(Index); Receivers[Index]->Interactable->InteractionTime = 0.f;
            Interaction->SetViewedInteractable(Receivers[Index]->Interactable);
            Interaction->BeginInteract(); Interaction->EndInteract();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRelayVictoryOrderTest,
    "ProjectVelkorran.Campaign.RelayReceiver.BothPhysicalReceiversAndVictoryInEitherOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRelayVictoryOrderTest::RunTest(const FString& Parameters)
{
    for (bool bDefeatsFirst : {false, true})
    {
        SovRelayTests::FWorld F; if (!TestNotNull(TEXT("Ready relay fixture"), F.ASC) || !F.Start()) { AddError(F.Error); return false; }
        auto* State = F.PC->GetCampaignState();
        TestEqual(TEXT("Generic CompleteBeat cannot fabricate receivers or battle proof"), State->CompleteBeat(TEXT("RelayOverlook")), ESovCampaignResult::Invalid);
        if (bDefeatsFirst)
        {
            F.KillGuard(); F.FrameOnce();
            TestEqual(TEXT("All hostiles dead does not complete the receiver objective"), State->GetJournal().Num(), 0);
            TestTrue(TEXT("Pending receivers retain the director's save fence"), F.Director->IsCampaignReceiptPending());
        }
        F.Operate(0); TestTrue(TEXT("Ordinary Narrative input reserved physical receiver request"), F.Receivers[0]->IsRequestPending());
        F.FrameOnce();
        TestTrue(TEXT("First receiver disabled by native interaction"), F.Receivers[0]->IsDisabled());
        TestFalse(TEXT("First receipt cannot disable the other receiver"), F.Receivers[1]->IsDisabled());
        TestEqual(TEXT("One receiver never awards the beat"), State->GetJournal().Num(), 0);
        FText Error; TestFalse(TEXT("Duplicate disable cannot produce a second receipt"), F.Receivers[0]->RequestUse(F.Player, Error));
        F.Operate(1); F.FrameOnce();
        if (!bDefeatsFirst)
        {
            TestEqual(TEXT("Two receivers do not manufacture hostile defeat"), State->GetJournal().Num(), 0);
            F.KillGuard();
        }
        F.FrameOnce();
        if (!TestEqual(TEXT("Both native requirements produce one event"), State->GetJournal().Num(), 1))
        { AddError(F.Objective->LastError + TEXT(" / ") + F.Receivers[1]->LastError.ToString()); return false; }
        TestEqual(TEXT("Journal persists exactly two receiver identities"), State->GetJournal()[0].DisabledReceiverIds.Num(), 2);
        TestFalse(TEXT("Committed receivers and victory release save fence"), F.Director->IsCampaignReceiptPending());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRelayIdentityAndReachTest,
    "ProjectVelkorran.Campaign.RelayReceiver.DuplicateIdentityMissingActorDistanceAndLOSFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRelayIdentityAndReachTest::RunTest(const FString& Parameters)
{
    SovRelayTests::FWorld F; if (!TestNotNull(TEXT("Ready relay fixture"), F.ASC)) { return false; }
    F.Objective->RequiredReceivers[1] = F.Receivers[0]; TestFalse(TEXT("Same actor cannot satisfy two required receivers"), F.Start());
    F.Objective->RequiredReceivers[1] = F.Receivers[1]; F.Receivers[1]->ReceiverId = F.Receivers[0]->ReceiverId;
    TestFalse(TEXT("Same receiver ID cannot count twice"), F.Start()); F.Receivers[1]->ReceiverId = TEXT("East");
    if (!TestTrue(TEXT("Distinct complete authoring starts"), F.Start())) { AddError(F.Error); return false; }
    FText Error;
    F.Player->SetActorLocation(FVector(-5000, 0, 0)); TestFalse(TEXT("Remote native calls cannot disable a receiver"), F.Receivers[0]->RequestUse(F.Player, Error));
    F.Approach(0);
    auto* Blocker = F.World->SpawnActor<AActor>(); auto* Shape = NewObject<UBoxComponent>(Blocker);
    Blocker->SetRootComponent(Shape); Blocker->AddInstanceComponent(Shape); Shape->SetBoxExtent(FVector(15, 150, 200));
    Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent();
    Blocker->SetActorLocation(F.Player->GetActorLocation() + FVector(90, 0, 0));
    TestFalse(TEXT("Occluded receiver cannot be disabled through a wall"), F.Receivers[0]->RequestUse(F.Player, Error));
    Blocker->Destroy();
    F.Operate(0); F.Receivers[1]->Destroy(); F.FrameOnce(); F.KillGuard(); F.FrameOnce();
    TestEqual(TEXT("Lost second actor retires proof instead of completing the beat"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    TestTrue(TEXT("Lost authoring retains uncommitted-victory fence"), F.Director->IsCampaignReceiptPending());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRelayRetirementTest,
    "ProjectVelkorran.Campaign.RelayReceiver.ReadinessChangeAndRetryRetireOldRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRelayRetirementTest::RunTest(const FString& Parameters)
{
    {
        SovRelayTests::FWorld F; if (!TestNotNull(TEXT("Ready relay fixture"), F.ASC) || !F.Start()) { AddError(F.Error); return false; }
        F.Operate(0); F.ASC->SetCharacterReadyEpoch(F.ASC->GetCharacterReadyEpoch() + 1); F.FrameOnce();
        TestFalse(TEXT("Later readiness epoch retires queued physical request"), F.Receivers[0]->IsDisabled());
        TestEqual(TEXT("Stale request writes no campaign fact"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    }
    {
        SovRelayTests::FWorld F; if (!TestNotNull(TEXT("Ready relay fixture"), F.ASC) || !F.Start()) { AddError(F.Error); return false; }
        F.Operate(0); F.FrameOnce(); TestTrue(TEXT("Receiver disabled in failed attempt"), F.Receivers[0]->IsDisabled());
        F.Director->FailEncounter(); TestFalse(TEXT("Failure withdraws that attempt's receiver state"), F.Receivers[0]->IsDisabled());
        if (!TestTrue(TEXT("Production entry retry is accepted"), F.Start())) { AddError(F.Error); return false; }
        for (int32 Step = 0; Step < 8 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Step)
        { F.FrameOnce(); F.Director->Tick(.016f); }
        if (!TestEqual(TEXT("Retry reopens a fresh encounter"), F.Director->GetEncounterState(), ESovEncounterState::Active)) { return false; }
        TestFalse(TEXT("Retry never imports the old partial receiver receipt"), F.Receivers[0]->IsDisabled());
        F.KillGuard(); F.FrameOnce();
        TestEqual(TEXT("Fresh attempt still requires both receivers"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRelayJournalRestoreTest,
    "ProjectVelkorran.Campaign.RelayReceiver.JournalRestoresBothDisabledStatesAndRejectsMissingReceipt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRelayJournalRestoreTest::RunTest(const FString& Parameters)
{
    SovRelayTests::FWorld F; if (!TestNotNull(TEXT("Ready relay fixture"), F.ASC) || !F.Start()) { AddError(F.Error); return false; }
    F.Operate(0); F.FrameOnce(); F.Operate(1); F.FrameOnce(); F.KillGuard(); F.FrameOnce();
    auto* State = F.PC->GetCampaignState();
    if (!TestEqual(TEXT("Native battle/receiver fact exists"), State->GetJournal().Num(), 1)) { return false; }
    FNarrativeSaveComponent Record; FString Error;
    TestTrue(TEXT("Existing Narrative serializer captures physical receiver receipts"), USovEncounterSnapshotLibrary::CaptureComponent(State, Record));
    TestTrue(TEXT("Valid receiver receipt replays"), USovCampaignStateComponent::ValidateSerializedSave(Record.ByteData, Error));
    TestTrue(TEXT("Native campaign restore succeeds"), USovEncounterSnapshotLibrary::RestoreComponent(State, Record));
    TestTrue(TEXT("West receiver derives disabled state from restored journal"), F.Receivers[0]->IsDisabled());
    TestTrue(TEXT("East receiver derives disabled state from restored journal"), F.Receivers[1]->IsDisabled());
    auto& Journal = const_cast<TArray<FSovCampaignJournalEntry>&>(State->GetJournal());
    Journal[0].DisabledReceiverIds.Remove(TEXT("East"));
    TestTrue(TEXT("Tampered state captured for replay validation"), USovEncounterSnapshotLibrary::CaptureComponent(State, Record));
    TestFalse(TEXT("Missing physical receipt rejects saved campaign replay"), USovCampaignStateComponent::ValidateSerializedSave(Record.ByteData, Error));
    return true;
}
#endif
