// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovEncounterObjectiveRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignInteractionTerminal.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovAurelionCrucibleDirector.h"
#include "Sovereign/SovGameplayTags.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Character/PlayerDefinition.h"
#include "AI/NPCDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/Script.h"

FGameplayTag ASovCrucibleTestSelenePawn::GetProtagonistIdentityTag() const
{ return FSovGameplayTags::Get().Character_Player_Selene; }
ETeamAttitude::Type ASovCrucibleTestSelenePawn::GetTeamAttitudeTowards(const AActor& Other) const
{ return Other.IsA<ASovNPCCharacterBase>() ? ETeamAttitude::Hostile : ETeamAttitude::Friendly; }

#if WITH_AUTOMATION_TESTS
/** Tests the native roster-transfer primitive after real phase completion. The public API's
 * actual controller handoff prerequisite is exercised separately by refusal assertions. */
struct FSovCrucibleRuntimeTestAccess
{
    static void Step(ASovEncounterDirector* Director, float DeltaSeconds)
    { Director->Tick(DeltaSeconds); }
    static bool Transfer(ASovAurelionLinkPhaseDirector* Source, ASovAurelionThermalPhaseDirector* Destination, FString& Error)
    { return Source->TransferFrozenParticipants(Destination, Error); }
};
namespace
{
    struct FEncounterObjectiveWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovHandoffRuntimeTestController* PC = nullptr;
        ASovHandoffRuntimeTestPawn* Player = nullptr;
        UNarrativeAbilitySystemComponent* ASC = nullptr;
        USovCampaignDefinition* Mission = nullptr;
        ASovEncounterDirector* Director = nullptr;
        ASovCampaignEncounterObjective* Objective = nullptr;
        FString SetupError;
        uint64 Frame = GFrameCounter;
        FEncounterObjectiveWorld(bool bCrucible = false)
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeActorsForPlay(FURL()); World->GetTimerManager().Tick(0.f);
            PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
            Player = bCrucible ? World->SpawnActor<ASovCrucibleTestSelenePawn>() : World->SpawnActor<ASovHandoffRuntimeTestPawn>();
            auto* PS = World->SpawnActor<ASovPlayerState>();
            if (!PC || !Player || !PS) { return; }
            World->AddController(PC);
            auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            Player->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Player);
            if (!Player->StageTestReadiness(PS, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
            ASC = Player->GetNarrativeAbilitySystemComponent();
            Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
            Mission->MissionId = TEXT("M12_EncounterObjectiveTest"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
            Mission->PawnClass = Player->GetClass(); Mission->PlayerDefinition = Definition;
            FSovCampaignBeatDefinition Hold; Hold.BeatId = TEXT("HoldMixedSurvivorCorridor");
            Hold.RequiredProtagonist = Mission->Protagonist; Hold.RequiredEncounterId = TEXT("Test.MixedSurvivorCorridor");
            Hold.MinimumProtectedParticipants = 2;
            Hold.RequiredEncounterProof = bCrucible ? ESovEncounterProofType::AurelionLinks : ESovEncounterProofType::RequiredDefeats;
            Hold.ObjectiveText = FText::FromString(TEXT("Protect both survivors and defeat the formation"));
            FSovCampaignBeatDefinition Exit; Exit.BeatId = TEXT("SecureTarrikRoute"); Exit.PrerequisiteBeats = { Hold.BeatId };
            Exit.ObjectiveText = FText::FromString(TEXT("Secure the exit")); Mission->Beats = { Hold, Exit };
            if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { ASC = nullptr; return; }
            Director = bCrucible ? World->SpawnActor<ASovAurelionLinkPhaseDirector>() : World->SpawnActor<ASovEncounterDirector>();
            Director->EncounterId = Hold.RequiredEncounterId;
            auto* NPCDefinition = NewObject<UNPCDefinition>(PC); PC->KeepAlive.Add(NPCDefinition);
            TArray<FName> ParticipantIds = { TEXT("Formation.Guard"), TEXT("Survivor.Dominion"), TEXT("Survivor.Reformation") };
            if (bCrucible) { ParticipantIds.Append({ TEXT("Crucible.NodeWest"), TEXT("Crucible.NodeEast") }); }
            for (const FName Id : ParticipantIds)
            {
                FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                auto* NPC = World->SpawnActor<ASovCampaignMassRoundTripNPC>(ASovCampaignMassRoundTripNPC::StaticClass(),
                    FVector(600.f + Director->Participants.Num() * 300.f, 0, 0), FRotator::ZeroRotator, Spawn);
                if (!NPC) { ASC = nullptr; return; }
                NPC->SetNPCDefinition(NPCDefinition);
                auto* NPCASC = NPC->GetNarrativeAbilitySystemComponent();
#define SOV_RESOURCE(Name) NPCASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMax##Name##Attribute(), 100.f); NPCASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), 100.f);
                SOV_RESOURCE(Health) SOV_RESOURCE(Shield) SOV_RESOURCE(Stamina) SOV_RESOURCE(Poise) SOV_RESOURCE(Echo)
#undef SOV_RESOURCE
                const bool bRequired = Id != TEXT("Survivor.Dominion") && Id != TEXT("Survivor.Reformation");
                if (!Director->RegisterParticipant(Id, NPC, bRequired)) { ASC = nullptr; return; }
                if (!bRequired) { Director->ProtectedParticipantIds.Add(Id); }
            }
            if (auto* Crucible = Cast<ASovAurelionLinkPhaseDirector>(Director))
            {
                Crucible->MissionId = Mission->MissionId; Crucible->CompletionBeat = Hold.BeatId;
                Crucible->EliteParticipantId = TEXT("Formation.Guard");
                for (const FName NodeId : { FName(TEXT("Crucible.NodeWest")), FName(TEXT("Crucible.NodeEast")) })
                {
                    auto* Node = Director->GetParticipant(NodeId);
                    auto* Link = NewObject<USovCommandLinkComponent>(Node, TEXT("CrucibleLink"));
                    Node->AddInstanceComponent(Link); Link->RegisterComponent();
                    Link->ConfigureLinkId(NodeId); Link->RegisterLinkedActor(Director->GetParticipant(TEXT("Formation.Guard")));
                    if (!Link->ActivateCommandLink(Node)) { ASC = nullptr; return; }
                    FSovAurelionCrucibleLink Binding; Binding.ParticipantId = NodeId; Binding.ComponentName = Link->GetFName(); Binding.LinkId = NodeId;
                    Crucible->RequiredLinks.Add(Binding);
                }
            }
            Objective = World->SpawnActor<ASovCampaignEncounterObjective>();
            Objective->EncounterDirector = Director; Objective->MissionId = Mission->MissionId; Objective->CompletionBeat = Hold.BeatId;
            auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
            if (!Save || !Save->UpdateSaveObject(true)) { ASC = nullptr; return; }
        }
        ~FEncounterObjectiveWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        bool Start() { return Objective && Objective->StartEncounter(Player, SetupError); }
        void Kill(FName Id)
        {
            auto* NPC = Director->GetParticipant(Id); if (!NPC) { return; }
            auto* NPCASC = CastChecked<USovCoordinationTestASC>(NPC->GetNarrativeAbilitySystemComponent());
            // Only the external combat death is supplied. The director's live delegate binding,
            // participant receipt, victory/protection evaluation and journal publication are native.
            NPCASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
            NPCASC->SeedDead(true); NPCASC->OnDeathStateChanged.Broadcast(NPC, NPCASC, true);
        }
        void NextFrame()
        { TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame); World->GetTimerManager().Tick(.016f); }
        USovEncounterObjectiveTestObserver* Observer()
        { auto* O = NewObject<USovEncounterObjectiveTestObserver>(PC); PC->KeepAlive.Add(O); return O; }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterObjectiveNativeVictoryTest,
    "ProjectVelkorran.Campaign.EncounterObjective.NativeVictoryCommitsExactlyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterObjectiveNativeVictoryTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.ASC)) { return false; }
    F.Director->ProtectedParticipantIds.Remove(TEXT("Survivor.Reformation"));
    TestFalse(TEXT("A protection objective cannot start with fewer than its required survivors"), F.Start());
    TestFalse(TEXT("An underspecified protection encounter never captures an entry"), F.Director->HasEncounterPlayer(F.Player));
    F.Director->ProtectedParticipantIds.Add(TEXT("Survivor.Reformation"));
    if (!TestTrue(TEXT("Production entry capture and encounter begin succeed"), F.Start())) { AddError(F.SetupError); return false; }
    TestTrue(TEXT("Director captured the real player's entry"), F.Director->HasEncounterPlayer(F.Player));
    TestFalse(TEXT("A manual completion cannot skip a living required guard"), F.Director->CompleteEncounter());
    TestEqual(TEXT("Raw campaign completion cannot invent the encounter receipt"),
        F.PC->GetCampaignState()->CompleteBeat(F.Objective->CompletionBeat), ESovCampaignResult::Invalid);
    auto* Terminal = F.World->SpawnActor<ASovCampaignInteractionTerminal>();
    Terminal->TerminalId = TEXT("CannotBypassCombat"); Terminal->MissionId = F.Mission->MissionId;
    Terminal->CompletionBeat = F.Objective->CompletionBeat; Terminal->bWriteCheckpoint = false;
    Terminal->SetActorLocation(F.Player->GetActorLocation() + FVector(180, 0, 0)); FText Error;
    TestFalse(TEXT("A physical generic terminal cannot bypass the battle"), Terminal->CanUse(F.Player, Error));
    auto* Observer = F.Observer(); ESovCampaignResult Recursive = ESovCampaignResult::Applied;
    Observer->OnCommit = [&]() { Recursive = F.PC->GetCampaignState()->CompleteBeat(F.Objective->CompletionBeat); };
    F.PC->GetCampaignState()->OnBeatCommitted.AddDynamic(Observer, &USovEncounterObjectiveTestObserver::BeatCommitted);
    const FGuid Attempt = F.Director->GetAttemptId(); F.Kill(TEXT("Formation.Guard"));
    TestEqual(TEXT("Actual required death resolves the director"), F.Director->GetEncounterState(), ESovEncounterState::Succeeded);
    TestTrue(TEXT("Victory has confirmed hostile and survivor proof"), F.Director->HasConfirmedVictory());
    TestTrue(TEXT("Save admission waits for deferred campaign receipt"), F.Objective->IsResultPending());
    TestEqual(TEXT("Director callback has not recursively written a journal event"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    F.NextFrame();
    const auto& Journal = F.PC->GetCampaignState()->GetJournal();
    if (!TestEqual(TEXT("Exactly one event commits"), Journal.Num(), 1)) { AddError(F.Objective->LastError); return false; }
    TestEqual(TEXT("Receipt stores the real encounter ID"), Journal[0].EncounterId, F.Director->EncounterId);
    TestEqual(TEXT("Receipt stores the real attempt"), Journal[0].EncounterAttemptId, Attempt);
    TestEqual(TEXT("Recursive campaign mutation is rejected"), Recursive, ESovCampaignResult::Busy);
    TestFalse(TEXT("Committed receipt releases save admission"), F.Objective->IsResultPending());
    F.Director->OnEncounterStateChanged.Broadcast(ESovEncounterState::Active, ESovEncounterState::Succeeded); F.NextFrame();
    TestEqual(TEXT("Replayed success notification cannot repeat the fact"), Journal.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterObjectiveProtectionTest,
    "ProjectVelkorran.Campaign.EncounterObjective.ProtectedDeathAndDestructionNeverBecomeVictory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterObjectiveProtectionTest::RunTest(const FString& Parameters)
{
    for (const bool bDestroy : { false, true })
    {
        FEncounterObjectiveWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
        FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f);
        TestTrue(TEXT("Protection monitoring remains scheduled after an ordinary tick"), F.Director->IsActorTickEnabled());
        if (bDestroy) { F.Director->GetParticipant(TEXT("Survivor.Reformation"))->Destroy(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
        else { F.Kill(TEXT("Survivor.Reformation")); }
        TestEqual(TEXT("Protected loss fails the attempt"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
        F.Kill(TEXT("Formation.Guard")); F.NextFrame();
        TestFalse(TEXT("Hostile death after survivor failure is not victory"), F.Director->HasConfirmedVictory());
        TestEqual(TEXT("Failure never writes campaign success"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
        TestFalse(TEXT("Failed result does not hold an uncommitted-victory save gate"), F.Objective->IsResultPending());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterObjectiveRetiredResultTest,
    "ProjectVelkorran.Campaign.EncounterObjective.CallbackReadinessAndLoadRetireVictory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterObjectiveRetiredResultTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 5; ++Case)
    {
        FEncounterObjectiveWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
        auto* Observer = F.Observer();
        Observer->OnVictory = [&]()
        {
            if (Case == 0) { F.ASC->SetCharacterReadyEpoch(F.ASC->GetCharacterReadyEpoch() + 1); }
            else if (Case == 1) { F.PC->UnPossess(); }
            else if (Case == 2) { F.Director->Load_Implementation(); }
            else if (Case == 3) { F.PC->GetCampaignState()->Load_Implementation(); }
            else { F.Objective->Destroy(); }
        };
        F.Director->OnEncounterStateChanged.AddDynamic(Observer, &USovEncounterObjectiveTestObserver::EncounterChanged);
        F.Kill(TEXT("Formation.Guard")); F.NextFrame();
        TestEqual(TEXT("Later success callback retirement prevents a deferred campaign award"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
        TestTrue(TEXT("The director retains the save fence even if the objective actor is destroyed"), F.Director->IsCampaignReceiptPending());
        F.NextFrame(); TestEqual(TEXT("Retired result stays retired"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterObjectiveRetryTest,
    "ProjectVelkorran.Campaign.EncounterObjective.NativeEntryRetryCreatesFreshAttempt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterObjectiveRetryTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    const FGuid FailedAttempt = F.Director->GetAttemptId();
    F.Kill(TEXT("Survivor.Dominion"));
    FString Error;
    if (!TestTrue(TEXT("Objective requests the production director checkpoint retry"), F.Objective->StartEncounter(F.Player, Error)))
    { AddError(Error); return false; }
    for (int32 Step = 0; Step < 8 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Step)
    { F.NextFrame(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
    if (!TestEqual(TEXT("All replacement participants restore before play"), F.Director->GetEncounterState(), ESovEncounterState::Active)) { return false; }
    TestTrue(TEXT("Entry retry creates a new attempt identity"), F.Director->GetAttemptId() != FailedAttempt);
    TestTrue(TEXT("Protected survivor is restored from its entry record"), F.Director->GetParticipant(TEXT("Survivor.Dominion"))->IsAlive());
    TestEqual(TEXT("Failure and restore add no success consequences"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    const FGuid RetriedAttempt = F.Director->GetAttemptId(); F.Kill(TEXT("Formation.Guard")); F.NextFrame();
    const auto& Journal = F.PC->GetCampaignState()->GetJournal();
    if (!TestEqual(TEXT("Only the successful retry writes a fact"), Journal.Num(), 1)) { AddError(F.Objective->LastError); return false; }
    TestEqual(TEXT("Fact belongs to the retry, never the failed attempt"), Journal[0].EncounterAttemptId, RetriedAttempt);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterObjectiveReceiptSaveTest,
    "ProjectVelkorran.Campaign.EncounterObjective.ReceiptSurvivesNarrativeSerializationAndRejectsTampering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterObjectiveReceiptSaveTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    F.Kill(TEXT("Formation.Guard")); F.NextFrame();
    auto* State = F.PC->GetCampaignState();
    if (!TestEqual(TEXT("Native result exists before saving"), State->GetJournal().Num(), 1)) { return false; }
    FNarrativeSaveComponent Record; FString Error;
    TestTrue(TEXT("Existing Narrative component capture serializes the receipt"), USovEncounterSnapshotLibrary::CaptureComponent(State, Record));
    TestTrue(TEXT("Native serialized campaign replay accepts the valid receipt"), USovCampaignStateComponent::ValidateSerializedSave(Record.ByteData, Error));
    TestTrue(TEXT("Existing component restore succeeds"), USovEncounterSnapshotLibrary::RestoreComponent(State, Record));
    TestTrue(TEXT("Restored state remains valid"), State->IsStateValid());
    TestEqual(TEXT("Restored fact cannot duplicate"), State->GetJournal().Num(), 1);
    auto& Journal = const_cast<TArray<FSovCampaignJournalEntry>&>(State->GetJournal());
    Journal[0].EncounterAttemptId.Invalidate();
    TestTrue(TEXT("Malformed test state can be serialized for validation"), USovEncounterSnapshotLibrary::CaptureComponent(State, Record));
    TestFalse(TEXT("An encounter fact without its native attempt is rejected on replay"), USovCampaignStateComponent::ValidateSerializedSave(Record.ByteData, Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCrucibleNativeLinkBoundaryTest,
    "ProjectVelkorran.Campaign.Aurelion.CrucibleNativeLinksFreezeWithoutKillingElite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCrucibleNativeLinkBoundaryTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true); if (!TestNotNull(TEXT("Ready Selene campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    auto* Crucible = CastChecked<ASovAurelionLinkPhaseDirector>(F.Director);
    auto* Elite = F.Director->GetParticipant(TEXT("Formation.Guard")); auto* EliteASC = Elite->GetNarrativeAbilitySystemComponent();
    EliteASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 73.f);
    EliteASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 41.f);
    TestFalse(TEXT("A scripted phase completion cannot substitute for either live link sever"), Crucible->CompleteEncounter());
    for (int32 Index = 0; Index < Crucible->RequiredLinks.Num(); ++Index)
    {
        auto* Node = Crucible->GetParticipant(Crucible->RequiredLinks[Index].ParticipantId);
        auto* Link = Node->FindComponentByClass<USovCommandLinkComponent>(); FSovCommandLinkSeverResult Result;
        TestEqual(TEXT("Selene performs a real authoritative native link sever"), Link->TrySeverCommandLink(F.Player, Result), ESovCommandLinkSeverResolution::NewlySevered);
        TestEqual(TEXT("Link callbacks never directly publish phase completion"), Crucible->GetEncounterState(), ESovEncounterState::Active);
        if (Index == 0) { FSovCrucibleRuntimeTestAccess::Step(Crucible, .016f); TestEqual(TEXT("One sever cannot finish the phase"), Crucible->GetEncounterState(), ESovEncounterState::Active); }
    }
    FSovCrucibleRuntimeTestAccess::Step(Crucible, .016f);
    TestEqual(TEXT("Both sever receipts settle into a completed phase boundary"), Crucible->GetEncounterState(), ESovEncounterState::Succeeded);
    TestTrue(TEXT("Elite remains living for phase B"), Elite->IsAlive());
    TestEqual(TEXT("Freeze preserves elite health"), EliteASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 73.f);
    TestEqual(TEXT("Freeze preserves elite shield"), EliteASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 41.f);
    F.NextFrame();
    const auto& Journal = F.PC->GetCampaignState()->GetJournal();
    if (!TestEqual(TEXT("Exactly one typed link-phase fact commits"), Journal.Num(), 1)) { AddError(F.Objective->LastError); return false; }
    TestEqual(TEXT("Journal preserves link proof rather than claiming kill-all"), Journal[0].EncounterProof, ESovEncounterProofType::AurelionLinks);
    TestTrue(TEXT("Only the exact frozen completed phase permits safe transition capture"), Crucible->IsCompletedPhaseBoundaryQuiescentForSave(F.Player));
    FString Error;
    TestFalse(TEXT("Selene cannot call phase B transfer without the actual Tarrik handoff"), Crucible->CompletePhaseHandoff(F.Player, Error));
    TestTrue(TEXT("Rejected transfer leaves the same elite under its source owner"), Crucible->GetParticipant(TEXT("Formation.Guard")) == Elite);
    // Isolate the transfer primitive from navigation and external companion assets: no journal or
    // handshake success is fabricated. This checks the exact ownership/save logic used after handoff.
    auto* PhaseB = F.World->SpawnActor<ASovAurelionThermalPhaseDirector>(); PhaseB->EncounterId = TEXT("Test.CrucibleB");
    PhaseB->EliteParticipantId = TEXT("Formation.Guard");
    TestTrue(TEXT("The completed native phase transfers its existing frozen roster"), FSovCrucibleRuntimeTestAccess::Transfer(Crucible, PhaseB, Error));
    TestTrue(TEXT("The destination receives the same elite actor"), PhaseB->GetParticipant(TEXT("Formation.Guard")) == Elite);
    TestEqual(TEXT("No source ownership remains"), Crucible->Participants.Num(), 0);
    TestEqual(TEXT("Transfer preserves elite health exactly"), EliteASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 73.f);
    TestEqual(TEXT("Transfer preserves elite shield exactly"), EliteASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 41.f);
    TestTrue(TEXT("Destination blocks saves until its entry snapshot is captured"), PhaseB->IsPhaseEntryCapturePending());
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>(); FNarrativeActorRecord SourceRecord;
    TestTrue(TEXT("Narrative stores source transfer ownership markers"), Save->CreateActorRecord(Crucible, SourceRecord));
    TestTrue(TEXT("Native source restore consumes its own saved record"), Save->LoadActorFromRecord(Crucible, SourceRecord));
    TestEqual(TEXT("Restoring phase A never reclaims transferred NPCs from phase B"), Crucible->Participants.Num(), 0);
    TestTrue(TEXT("Destination still owns the same elite after source restore"), PhaseB->GetParticipant(TEXT("Formation.Guard")) == Elite);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCruciblePrematureEliteDeathTest,
    "ProjectVelkorran.Campaign.Aurelion.CrucibleEliteDeathBeforePhaseBoundaryRequiresRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCruciblePrematureEliteDeathTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true); if (!TestNotNull(TEXT("Ready Selene campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    F.Kill(TEXT("Formation.Guard")); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); F.NextFrame();
    TestEqual(TEXT("Early elite kill has an explicit recoverable failure"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
    TestEqual(TEXT("An impossible phase B is never recorded as success"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

#endif
