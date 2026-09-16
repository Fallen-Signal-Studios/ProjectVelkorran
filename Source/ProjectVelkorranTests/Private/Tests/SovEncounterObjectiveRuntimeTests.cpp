// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovEncounterObjectiveRuntimeTestFixtures.h"
#include "Components/SovEchoComponent.h"
#include "Tests/SovCampaignTerminalRuntimeTestFixtures.h"
#include "Abilities/SovGameplayAbility_AurelionElite.h"
#include "AI/SovAurelionElitePolicy.h"
#include "Campaign/SovAurelionRequestActor.h"
#include "Components/TextRenderComponent.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignInteractionTerminal.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "AI/NarrativeNPCController.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "World/SovWorldTransitActor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NarrativeGameplayTags.h"
#include "Campaign/SovAurelionCrucibleDirector.h"
#include "Sovereign/SovGameplayTags.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Character/PlayerDefinition.h"
#include "AI/NPCDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Script.h"

FGameplayTag ASovCrucibleTestSelenePawn::GetProtagonistIdentityTag() const
{ return FSovGameplayTags::Get().Character_Player_Selene; }
ETeamAttitude::Type ASovCrucibleTestSelenePawn::GetTeamAttitudeTowards(const AActor& Other) const
{ return Other.IsA<ASovNPCCharacterBase>() ? ETeamAttitude::Hostile : ETeamAttitude::Friendly; }

ASovWaveReleaseTestNPC::ASovWaveReleaseTestNPC(const FObjectInitializer& Initializer) : Super(Initializer)
{ CreateDefaultSubobject<USovAurelionFreshCommandLink>(TEXT("FormationLink")); }

ASovEncounterStoryIdentityNPC::ASovEncounterStoryIdentityNPC(const FObjectInitializer& Initializer) : Super(Initializer)
{ Tags.Add(TEXT("Test.Story.Default")); }
void ASovEncounterStoryIdentityNPC::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    bHadStoryIdentityAtInitialization = ActorHasTag(TEXT("Aurelion_TrappedMarine")) || ActorHasTag(TEXT("Aurelion_Lyric"));
}

#if WITH_AUTOMATION_TESTS
/** Tests the native roster-transfer primitive after real phase completion. The public API's
 * actual controller handoff prerequisite is exercised separately by refusal assertions. */
struct FSovCrucibleRuntimeTestAccess
{
    static const TArray<FSovEncounterNPCRecord>& SavedEntry(const ASovEncounterDirector* Director)
    { return Director->EntryParticipants; }
    static const TSet<FName>& SavedDefeats(const ASovEncounterDirector* Director)
    { return Director->DefeatedParticipants; }
    static bool ProtectionValid(const ASovEncounterDirector* Director)
    { FString Error; return Director->ValidateProtectionConfiguration(Error); }
    static void CorruptSavedProtectionRole(ASovEncounterDirector* Director, FName Id)
    { for (auto& Record : Director->EntryParticipants) { if (Record.ParticipantId == Id) { Record.bRequiredForVictory = true; } } }
    static void ClearEntryActorTags(ASovEncounterDirector* Director)
    { for (auto& Record : Director->EntryParticipants) { Record.ActorTags.Reset(); } }
    static bool EntryHasActorTag(const ASovEncounterDirector* Director, FName ParticipantId, FName Tag)
    {
        const auto* Record = Director->EntryParticipants.FindByPredicate([ParticipantId](const auto& Entry) { return Entry.ParticipantId == ParticipantId; });
        return Record && Record->ActorTags.Contains(Tag);
    }
    static void Step(ASovEncounterDirector* Director, float DeltaSeconds)
    { Director->Tick(DeltaSeconds); }
    static void EntryStep(ASovEncounterDirector* Director) { Director->RefreshPreEntryHold(); }
    static void ReleaseDirectorHold(ASovEncounterDirector* Director) { Director->ReleaseSuspensions(); }
    static void LoadedHoldStep(ASovEncounterDirector* Director) { Director->RefreshLoadedParticipantHold(); }
    static bool HasLoadedHold(ASovEncounterDirector* Director) { return Director->bMaintainLoadedParticipantHold; }
    static void WaveStep(ASovEncounterDirector* Director)
    { Director->GetCoordinationComponent()->TickComponent(.1f, LEVELTICK_All, nullptr); }
    static void RetireWaveContext(ASovEncounterDirector* Director) { ++Director->RestoreGeneration; }
    static void ResetWaveForLoad(ASovEncounterDirector* Director) { Director->GetCoordinationComponent()->CurrentWave = 0; }
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
        FEncounterObjectiveWorld(bool bCrucible = false, bool bFormation = false, bool bAurelion = false, bool bStoryIdentity = false)
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
            // Retry uses the real capsule/ground clearance contract, so author a real safe entry.
            auto* Ground = World->SpawnActor<AActor>(); auto* GroundShape = NewObject<UBoxComponent>(Ground);
            Ground->SetRootComponent(GroundShape); Ground->AddInstanceComponent(GroundShape);
            GroundShape->SetBoxExtent(FVector(4000, 2000, 10)); GroundShape->SetCollisionProfileName(TEXT("BlockAll"));
            GroundShape->RegisterComponent(); Ground->SetActorLocation(FVector(0, 0, -10));
            Player->SetActorLocation(FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f));
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            ASC = Player->GetNarrativeAbilitySystemComponent();
            Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
            Mission->MissionId = bAurelion ? TEXT("M12_FireAndFrost") : TEXT("M12_EncounterObjectiveTest"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
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
                UClass* NPCClass = bFormation && Id == TEXT("Formation.Guard") ? ASovWaveReleaseTestNPC::StaticClass()
                    : bStoryIdentity && Id != TEXT("Formation.Guard") ? ASovEncounterStoryIdentityNPC::StaticClass() : ASovCampaignMassRoundTripNPC::StaticClass();
                auto* NPC = World->SpawnActor<ASovCampaignMassRoundTripNPC>(NPCClass,
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
    auto* Echo = F.Player->GetEchoComponent();
    if (!TestNotNull(TEXT("Campaign player owns Echo"), Echo)) { return false; }
    TestTrue(TEXT("Native director begin opens the Echo encounter scope"), Echo->IsEncounterActive());
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
    TestFalse(TEXT("Native director completion closes the Echo encounter scope"), Echo->IsEncounterActive());
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
        TestFalse(TEXT("Native director failure closes the Echo encounter scope"), F.Player->GetEchoComponent() && F.Player->GetEchoComponent()->IsEncounterActive());
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
    auto* RestoreObserver = F.Observer();
    F.Director->OnEncounterRestoreFailed.AddDynamic(RestoreObserver, &USovEncounterObjectiveTestObserver::RestoreFailed);
    const FGuid FailedAttempt = F.Director->GetAttemptId();
    F.Kill(TEXT("Survivor.Dominion"));
    FString Error;
    if (!TestTrue(TEXT("Objective requests the production director checkpoint retry"), F.Objective->StartEncounter(F.Player, Error)))
    { AddError(Error); return false; }
    for (int32 Step = 0; Step < 8 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Step)
    { F.NextFrame(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
    if (!TestEqual(TEXT("All replacement participants restore before play"), F.Director->GetEncounterState(), ESovEncounterState::Active)) { AddError(RestoreObserver->RestoreError); return false; }
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

namespace
{
    ASovCampaignMassRoundTripNPC* AddWaveNPC(FEncounterObjectiveWorld& F, FName Id)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* NPC = F.World->SpawnActor<ASovCampaignMassRoundTripNPC>(ASovCampaignMassRoundTripNPC::StaticClass(),
            FVector(600.f + F.Director->Participants.Num() * 300.f, 0, 0), FRotator::ZeroRotator, Spawn);
        NPC->SetNPCDefinition(F.Director->GetParticipant(TEXT("Formation.Guard"))->GetNPCDefinition());
        F.Director->RegisterParticipant(Id, NPC, true);
        return NPC;
    }
    void ClassifyWaves(FEncounterObjectiveWorld& F, const TSet<FName>& Future, int32 Cap)
    {
        auto* C = F.Director->GetCoordinationComponent(); C->MaximumCombatants = Cap; C->MaximumSupporting = 2;
        C->Composition.Reset();
        for (const auto& P : F.Director->Participants)
        {
            FSovEncounterCompositionMember M; M.ParticipantId = P.ParticipantId; M.Wave = Future.Contains(P.ParticipantId) ? 1 : 0;
            M.Tier = P.bRequiredForVictory ? ESovEncounterDecisionTier::Combatant : ESovEncounterDecisionTier::Supporting;
            C->Composition.Add(M);
        }
    }
    bool ConfigureFormation(FEncounterObjectiveWorld& F)
    {
        for (const FName Id : { FName(TEXT("Drone.A")), FName(TEXT("Drone.B")), FName(TEXT("Drone.C")), FName(TEXT("Reserve.A")), FName(TEXT("Reserve.B")) }) { AddWaveNPC(F, Id); }
        ClassifyWaves(F, { TEXT("Reserve.A"), TEXT("Reserve.B") }, 4);
        auto* Source = F.Director->GetParticipant(TEXT("Formation.Guard")); auto* Link = Source->FindComponentByClass<USovCommandLinkComponent>();
        if (!Link) { return false; }
        Link->ConfigureLinkId(TEXT("Aurelion.Formation")); Link->RegisterLinkedActor(F.Director->GetParticipant(TEXT("Drone.A")));
        if (!Link->ActivateCommandLink(Source)) { return false; }
        FSovEncounterWaveReleaseRule Rule; Rule.MaximumLivingReleasedHostiles = 2;
        Rule.CommandLinkParticipantId = TEXT("Formation.Guard"); Rule.CommandLinkComponentName = Link->GetFName();
        F.Director->GetCoordinationComponent()->WaveReleaseRules = { Rule };
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWaveFormationConjunctionTest,
    "ProjectVelkorran.Campaign.Encounter.Coordination.NativeFormationAndAliveCeiling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWaveFormationConjunctionTest::RunTest(const FString& Parameters)
{
    for (bool bSourceFirst : { false, true })
    {
        FEncounterObjectiveWorld F(false, true);
        if (!TestNotNull(TEXT("Ready formation fixture"), F.ASC) || !ConfigureFormation(F) || !F.Start()) { AddError(F.SetupError); return false; }
        auto* C = F.Director->GetCoordinationComponent(); auto* Reserve = F.Director->GetParticipant(TEXT("Reserve.A"));
        const FGuid Identity = Reserve->GetActorGUID_Implementation(); const FVector Position = Reserve->GetActorLocation();
        auto* ReserveASC = Reserve->GetNarrativeAbilitySystemComponent();
        ReserveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 63.f);
        TestTrue(TEXT("Future actor is staged before authentic proof"), Reserve->IsHidden());
        if (bSourceFirst) { F.Kill(TEXT("Formation.Guard")); }
        else { F.Kill(TEXT("Drone.A")); F.Kill(TEXT("Drone.B")); }
        FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
        TestEqual(TEXT("Neither count alone nor source defeat above the ceiling releases"), C->GetCurrentWave(), 0);
        if (bSourceFirst) { F.Kill(TEXT("Drone.A")); } else { F.Kill(TEXT("Formation.Guard")); }
        FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
        TestEqual(TEXT("Real source defeat AND native count release the wave"), C->GetCurrentWave(), 1);
        TestFalse(TEXT("Original actor becomes visible"), Reserve->IsHidden());
        TestEqual(TEXT("Original GUID retained"), Reserve->GetActorGUID_Implementation(), Identity);
        TestEqual(TEXT("Original transform retained"), Reserve->GetActorLocation(), Position);
        TestEqual(TEXT("Original resources retained"), ReserveASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 63.f);
        FSovCrucibleRuntimeTestAccess::WaveStep(F.Director); F.Kill(TEXT("Formation.Guard")); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
        TestEqual(TEXT("Repeated tick/death cannot release twice"), C->GetCurrentWave(), 1);
        TestFalse(TEXT("No staging lease remains on released actor"), ReserveASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWaveFormationRetryTest,
    "ProjectVelkorran.Campaign.Encounter.Coordination.FormationRetryRejectsFalseAndRetiredEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWaveFormationRetryTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(false, true);
    if (!TestNotNull(TEXT("Ready formation fixture"), F.ASC) || !ConfigureFormation(F) || !F.Start()) { AddError(F.SetupError); return false; }
    auto* C = F.Director->GetCoordinationComponent(); auto* Source = F.Director->GetParticipant(TEXT("Formation.Guard"));
    auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
    F.Kill(TEXT("Drone.A")); F.Kill(TEXT("Drone.B"));
    SourceASC->OnDeathStateChanged.Broadcast(Source, SourceASC, false);
    SourceASC->OnDeathStateChanged.Broadcast(Source, SourceASC, true); // Positive health: rejected by the director.
    FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
    TestEqual(TEXT("False/dead notification without native zero-health state is rejected"), C->GetCurrentWave(), 0);
    const FGuid FirstAttempt = F.Director->GetAttemptId(); F.Kill(TEXT("Formation.Guard"));
    FSovCrucibleRuntimeTestAccess::WaveStep(F.Director); TestEqual(TEXT("First real attempt releases"), C->GetCurrentWave(), 1);
    FString Error;
    auto* RestoreObserver = F.Observer();
    F.Director->OnEncounterRestoreFailed.AddDynamic(RestoreObserver, &USovEncounterObjectiveTestObserver::RestoreFailed);
    if (!TestTrue(TEXT("Production retry starts"), F.Director->RetryEncounter(Error))) { AddError(Error); return false; }
    for (int32 Step = 0; Step < 12 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Step)
    { F.NextFrame(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
    if (!TestEqual(TEXT("Native snapshot restoration returns Active"), F.Director->GetEncounterState(), ESovEncounterState::Active)) { AddError(RestoreObserver->RestoreError); return false; }
    TestTrue(TEXT("Retry owns a fresh attempt"), F.Director->GetAttemptId() != FirstAttempt);
    TestEqual(TEXT("Retry resets the wave"), C->GetCurrentWave(), 0);
    F.Kill(TEXT("Drone.A")); F.Kill(TEXT("Drone.B")); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
    TestEqual(TEXT("Old command-source defeat cannot satisfy a new attempt"), C->GetCurrentWave(), 0);
    F.Kill(TEXT("Formation.Guard")); FSovCrucibleRuntimeTestAccess::RetireWaveContext(F.Director); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
    TestEqual(TEXT("Retired lifecycle cannot release even with a current-looking death set"), C->GetCurrentWave(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWaveUnknownOwnershipTest,
    "ProjectVelkorran.Campaign.Encounter.Coordination.UnknownActorAndReleaseCallbackFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWaveUnknownOwnershipTest::RunTest(const FString& Parameters)
{
    for (bool bRetireOnRelease : { false, true })
    {
        FEncounterObjectiveWorld F(false, true);
        if (!TestNotNull(TEXT("Ready formation fixture"), F.ASC) || !ConfigureFormation(F) || !F.Start()) { AddError(F.SetupError); return false; }
        F.Kill(TEXT("Formation.Guard"));
        if (!bRetireOnRelease)
        {
            F.Director->GetParticipant(TEXT("Drone.A"))->Destroy(); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
            TestEqual(TEXT("Destroyed unconfirmed actor does not lower the living budget"), F.Director->GetCoordinationComponent()->GetCurrentWave(), 0);
        }
        else
        {
            auto* First = F.Director->GetParticipant(TEXT("Reserve.A"))->GetNarrativeAbilitySystemComponent();
            const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
            const FDelegateHandle Handle = First->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::AnyCountChange).AddLambda(
                [&](FGameplayTag, int32 Count) { if (Count == 0) { FSovCrucibleRuntimeTestAccess::RetireWaveContext(F.Director); } });
            F.Kill(TEXT("Drone.A")); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
            First->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::AnyCountChange).Remove(Handle);
            TestTrue(TEXT("First release callback retirement keeps the remaining staged actor held"), F.Director->GetParticipant(TEXT("Reserve.B"))->IsHidden());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWaveTransitEndpointTest,
    "ProjectVelkorran.Campaign.Encounter.Coordination.NativeDoorEndpointAndAliveCeiling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWaveTransitEndpointTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F;
    if (!TestNotNull(TEXT("Ready door fixture"), F.ASC)) { return false; }
    for (const FName Id : { FName(TEXT("Drone.A")), FName(TEXT("Drone.B")), FName(TEXT("Drone.C")), FName(TEXT("Reserve.A")), FName(TEXT("Reserve.B")), FName(TEXT("Reserve.C")) }) { AddWaveNPC(F, Id); }
    ClassifyWaves(F, { TEXT("Reserve.A"), TEXT("Reserve.B"), TEXT("Reserve.C") }, 6);
    auto* Door = F.World->SpawnActor<ASovWorldTransitActor>(); Door->TransitId = TEXT("RescueApproach");
    Door->SetActorLocation(FVector(180, 0, 132)); Door->TravelSeconds = .1f;
    F.Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    FSovEncounterWaveReleaseRule Rule; Rule.Condition = ESovEncounterWaveCondition::TransitDoorOpen;
    Rule.MaximumLivingReleasedHostiles = 3; Rule.TransitDoor = Door;
    auto* C = F.Director->GetCoordinationComponent(); C->WaveReleaseRules = { Rule };
    if (!F.Start()) { AddError(F.SetupError); return false; }
    FText Error;
    if (!TestTrue(TEXT("Actual native door use starts"), Door->RequestUse(F.Player, Error))) { AddError(Error.ToString()); return false; }
    static_cast<AActor*>(Door)->Tick(.05f); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
    TestFalse(TEXT("A partial native movement is not an open endpoint"), Door->IsOpenTraversableDoor());
    static_cast<AActor*>(Door)->Tick(.06f);
    TestTrue(TEXT("Native motion produces the actual traversable endpoint"), Door->IsOpenTraversableDoor());
    FSovCrucibleRuntimeTestAccess::WaveStep(F.Director); TestEqual(TEXT("Open route alone cannot exceed six active combatants"), C->GetCurrentWave(), 0);
    Door->SetPower(false); F.Kill(TEXT("Drone.A")); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
    TestEqual(TEXT("Disabled door and satisfied alive threshold still hold"), C->GetCurrentWave(), 0);
    Door->SetPower(true); Door->SetLockReason(FText::FromString(TEXT("Held"))); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
    TestEqual(TEXT("Locked endpoint does not count as an open approach"), C->GetCurrentWave(), 0);
    Door->SetLockReason(FText()); FSovCrucibleRuntimeTestAccess::WaveStep(F.Director);
    TestEqual(TEXT("Physical endpoint AND alive ceiling release exactly one wave"), C->GetCurrentWave(), 1);
    TestFalse(TEXT("All three original reinforcements are visible"), F.Director->GetParticipant(TEXT("Reserve.A"))->IsHidden()
        || F.Director->GetParticipant(TEXT("Reserve.B"))->IsHidden() || F.Director->GetParticipant(TEXT("Reserve.C"))->IsHidden());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWaveCrucibleReceiptTest,
    "ProjectVelkorran.Campaign.Aurelion.FirstAcceptedLinkReleasesBeforeFrozenTransfer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWaveCrucibleReceiptTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true); if (!TestNotNull(TEXT("Ready Selene fixture"), F.ASC)) { return false; }
    AddWaveNPC(F, TEXT("Weaver")); auto* Runner = AddWaveNPC(F, TEXT("WallRunner")); const FGuid RunnerId = Runner->GetActorGUID_Implementation();
    ClassifyWaves(F, { TEXT("WallRunner") }, 5);
    auto* C = F.Director->GetCoordinationComponent(); FSovEncounterWaveReleaseRule Rule;
    Rule.Condition = ESovEncounterWaveCondition::AcceptedCrucibleLink; Rule.MaximumLivingReleasedHostiles = 4; C->WaveReleaseRules = { Rule };
    if (!F.Start()) { AddError(F.SetupError); return false; }
    auto* Phase = CastChecked<ASovAurelionLinkPhaseDirector>(F.Director);
    TestFalse(TEXT("No accepted link proof exists at entry"), Phase->HasAcceptedCurrentLinkReceipt());
    auto* FirstLink = Phase->GetParticipant(Phase->RequiredLinks[0].ParticipantId)->FindComponentByClass<USovCommandLinkComponent>();
    FSovCommandLinkSeverResult Fake; Fake.LinkId = FirstLink->GetLinkId(); Fake.LinkOwner = FirstLink->GetOwner();
    Fake.LinkInstanceId = FirstLink->GetLinkInstanceId(); Fake.TransactionId = FGuid::NewGuid(); Fake.SeveredBy = F.Player; Fake.bEligibleForEchoReward = true;
    FirstLink->OnCommandLinkSevered.Broadcast(Fake); FSovCrucibleRuntimeTestAccess::WaveStep(Phase);
    TestEqual(TEXT("An unaccepted raw link callback cannot release reinforcements"), C->GetCurrentWave(), 0);
    for (int32 Index = 0; Index < Phase->RequiredLinks.Num(); ++Index)
    {
        auto* Link = Phase->GetParticipant(Phase->RequiredLinks[Index].ParticipantId)->FindComponentByClass<USovCommandLinkComponent>(); FSovCommandLinkSeverResult Actual;
        TestEqual(TEXT("Actual native Selene sever accepted"), Link->TrySeverCommandLink(F.Player, Actual), ESovCommandLinkSeverResolution::NewlySevered);
        TestTrue(TEXT("First current accepted receipt is observable"), Phase->HasAcceptedCurrentLinkReceipt());
    }
    FSovCrucibleRuntimeTestAccess::Step(Phase, .016f);
    TestEqual(TEXT("Both links before wave tick cannot freeze a hidden reinforcement"), Phase->GetEncounterState(), ESovEncounterState::Active);
    FSovCrucibleRuntimeTestAccess::WaveStep(Phase); TestEqual(TEXT("Accepted receipt releases the existing Wall-runner"), C->GetCurrentWave(), 1);
    TestFalse(TEXT("Wall-runner is presented"), Runner->IsHidden());
    FSovCrucibleRuntimeTestAccess::Step(Phase, .016f); F.NextFrame();
    TestEqual(TEXT("Complete released roster settles into a successful phase"), Phase->GetEncounterState(), ESovEncounterState::Succeeded);
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>(); FNarrativeActorRecord CompletedRecord;
    if (!TestTrue(TEXT("Native completed phase and wave are saved"), Save->CreateActorRecord(Phase, CompletedRecord))) { return false; }
    FSovCrucibleRuntimeTestAccess::ResetWaveForLoad(Phase);
    TestTrue(TEXT("Loaded completed phase restores its committed wave without replaying a sever"), Save->LoadActorFromRecord(Phase, CompletedRecord));
    TestEqual(TEXT("Saved completed wave is restored"), C->GetCurrentWave(), 1);
    auto* PhaseB = F.World->SpawnActor<ASovAurelionThermalPhaseDirector>(); PhaseB->EncounterId = TEXT("Test.WavePhaseB"); PhaseB->EliteParticipantId = Phase->EliteParticipantId;
    FString Error; if (!TestTrue(TEXT("Frozen native roster transfers"), FSovCrucibleRuntimeTestAccess::Transfer(Phase, PhaseB, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Transfer keeps Wall-runner identity"), PhaseB->GetParticipant(TEXT("WallRunner"))->GetActorGUID_Implementation(), RunnerId);
    TestTrue(TEXT("Destination composition accepts the retired source coordinator"), PhaseB->GetCoordinationComponent()->ValidateComposition(Error));
    TestTrue(TEXT("Director freeze survives attack-coordinator retirement"), Runner->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterPreEntryReadinessTest,
    "ProjectVelkorran.Campaign.Encounter.PreEntryHoldPreservesReadinessAndWaveOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterPreEntryReadinessTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(false, true);
    if (!TestNotNull(TEXT("Ready campaign"), F.ASC) || !ConfigureFormation(F)) { return false; }
    auto* Guard = CastChecked<ASovWaveReleaseTestNPC>(F.Director->GetParticipant(TEXT("Formation.Guard")));
    auto* GuardASC = Guard->GetNarrativeAbilitySystemComponent(); const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
    auto* Controller = F.World->SpawnActor<ANarrativeNPCController>(); Controller->Possess(Guard);
    Guard->SetNPCDefinition(Guard->GetNPCDefinition()); Controller->bRequireThreatMemoryForTargeting = true;
    if (!TestTrue(TEXT("Ready controller retains the already-established authored formation"), Guard->FindComponentByClass<USovCommandLinkComponent>()->IsCommandLinkActive())) { return false; }
    const auto Observe = [&]() { return Controller->ReportThreatObservation(F.Player, ENarrativeThreatSource::Damage, F.Player->GetActorLocation(), 1.f, 1.f, 8.f); };
    TestTrue(TEXT("Live controller initially admits native threat"), Observe());
    Guard->SetSnapshotReadyForTest(false); F.Director->bHoldParticipantsBeforeEntry = true;
    F.Director->bHoldNonVictoryParticipantsBeforeEntry = false;
    FSovCrucibleRuntimeTestAccess::EntryStep(F.Director);
    TestFalse(TEXT("Not-ready pawn receives no Busy tag that could block initialization"), GuardASC->HasMatchingGameplayTag(Busy));
    Guard->SetNPCDefinition(Guard->GetNPCDefinition());
    if (!Guard->FindComponentByClass<USovCommandLinkComponent>()->IsCommandLinkActive()) { Guard->FindComponentByClass<USovCommandLinkComponent>()->ActivateCommandLink(Guard); }
    FSovCrucibleRuntimeTestAccess::EntryStep(F.Director);
    TestTrue(TEXT("NPC readiness remains true under the hold"), Guard->IsEncounterSnapshotReady());
    TestEqual(TEXT("Director adds exactly one Busy contribution"), GuardASC->GetTagCount(Busy), 1);
    TestTrue(TEXT("Existing director threat lease blocks pre-entry combat"), Controller->IsThreatMemorySuspended());
    TestFalse(TEXT("Held controller cannot acquire fresh observations"), Observe());
    TestTrue(TEXT("Fresh links may admit only the exact committed pre-entry owner"), F.Director->IsOwnedPreEntryHold(Guard));
    auto* ExternalHold = F.Observer(); Controller->SetThreatMemorySuspended(ExternalHold, true);
    TestFalse(TEXT("Another threat suspension owner is never admitted for bootstrap"), F.Director->IsOwnedPreEntryHold(Guard));
    Controller->SetThreatMemorySuspended(ExternalHold, false);
    TestTrue(TEXT("Exclusive director ownership returns after independent release"), F.Director->IsOwnedPreEntryHold(Guard));
    auto* Reserve = F.Director->GetParticipant(TEXT("Reserve.A"));
    TestFalse(TEXT("Pre-entry hold does not hide a wave before checkpoint capture"), Reserve->IsHidden());
    auto* Survivor = F.Director->GetParticipant(TEXT("Survivor.Dominion"));
    auto* SurvivorASC = Survivor->GetNarrativeAbilitySystemComponent();
    TestFalse(TEXT("Opted-out story cast remains free before checkpoint capture"), SurvivorASC->HasMatchingGameplayTag(Busy));
    TestFalse(TEXT("Unheld story cast cannot claim an exclusive bootstrap lease"), F.Director->IsOwnedPreEntryHold(Survivor));
    SurvivorASC->AddLooseGameplayTag(Busy);
    TestFalse(TEXT("Checkpoint still checks the complete protected roster for quiescence"), F.Start());
    SurvivorASC->RemoveLooseGameplayTag(Busy);
    GuardASC->AddLooseGameplayTag(Busy);
    TestFalse(TEXT("Another Busy contribution prevents fresh link admission"), F.Director->IsOwnedPreEntryHold(Guard));
    TestFalse(TEXT("External Busy cannot be laundered through the director's admission"), F.Start());
    GuardASC->RemoveLooseGameplayTag(Busy);
    if (!TestTrue(TEXT("Actual checkpoint capture admits only its owned hold and starts"), F.Start())) { AddError(F.SetupError); return false; }
    TestEqual(TEXT("Begin releases the current actor's director Busy contribution"), GuardASC->GetTagCount(Busy), 0);
    TestFalse(TEXT("Normal begin releases the threat hold"), Controller->IsThreatMemorySuspended());
    TestFalse(TEXT("Active encounters cannot claim an inactive pre-entry lease"), F.Director->IsOwnedPreEntryHold(Guard));
    TestFalse(TEXT("Old pre-entry memory is not resurrected"), Controller->CanDirectlyTargetThreat(F.Player));
    TestTrue(TEXT("Fresh active encounter threat may be admitted"), Observe());
    TestTrue(TEXT("Future wave is now held by the existing coordination owner"), Reserve->IsHidden());
    TestEqual(TEXT("Future wave keeps only its staging Busy count"), Reserve->GetNarrativeAbilitySystemComponent()->GetTagCount(Busy), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterPreEntryRetirementTest,
    "ProjectVelkorran.Campaign.Encounter.PreEntryHoldRetirementAndReplacementLease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterPreEntryRetirementTest::RunTest(const FString& Parameters)
{
    {
        FEncounterObjectiveWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.ASC)) { return false; }
        F.Director->bHoldParticipantsBeforeEntry = true;
        auto* First = F.Director->GetParticipant(TEXT("Formation.Guard"))->GetNarrativeAbilitySystemComponent(); const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
        const FDelegateHandle Handle = First->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::AnyCountChange).AddLambda(
            [&](FGameplayTag, int32 Count) { if (Count == 1) { FSovCrucibleRuntimeTestAccess::RetireWaveContext(F.Director); } });
        FSovCrucibleRuntimeTestAccess::EntryStep(F.Director);
        First->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::AnyCountChange).Remove(Handle);
        TestFalse(TEXT("Retired callback stops before holding another participant"), F.Director->GetParticipant(TEXT("Survivor.Dominion"))->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(Busy));
        FSovCrucibleRuntimeTestAccess::ReleaseDirectorHold(F.Director);
    }
    {
        FEncounterObjectiveWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.ASC)) { return false; }
        auto* Guard = F.Director->GetParticipant(TEXT("Formation.Guard")); auto* Controller = F.World->SpawnActor<ANarrativeNPCController>();
        Controller->Possess(Guard); F.Director->bHoldParticipantsBeforeEntry = true; FSovCrucibleRuntimeTestAccess::EntryStep(F.Director);
        TestTrue(TEXT("Director owns old controller/pawn hold"), Controller->IsThreatMemorySuspended());
        auto* Replacement = F.World->SpawnActor<ASovCampaignMassRoundTripNPC>(); Replacement->SetNPCDefinition(Guard->GetNPCDefinition()); Controller->Possess(Replacement);
        auto* ExternalOwner = F.Observer(); Controller->SetThreatMemorySuspended(ExternalOwner, true);
        FSovCrucibleRuntimeTestAccess::ReleaseDirectorHold(F.Director);
        TestTrue(TEXT("Old director cleanup cannot release the replacement's external lease"), Controller->IsThreatMemorySuspended());
        TestFalse(TEXT("Old director never adds Busy to an unregistered replacement"), Replacement->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
        Controller->SetThreatMemorySuspended(ExternalOwner, false);
        TestFalse(TEXT("Replacement lease remains independently releasable"), Controller->IsThreatMemorySuspended());
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLoadedEncounterEntryHoldTest,
    "ProjectVelkorran.Campaign.Encounter.LoadedEntryRemainsFrozenUntilExplicitRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovLoadedEncounterEntryHoldTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F;
    if (!TestNotNull(TEXT("Ready campaign"), F.ASC)) { return false; }
    const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
    const auto Protection = FNarrativeGameplayTags::Get().State_Invulnerable;
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
    auto* Guard = F.Director->GetParticipant(TEXT("Formation.Guard"));
    auto* GuardASC = Guard->GetNarrativeAbilitySystemComponent();
    auto* Controller = F.World->SpawnActor<ANarrativeNPCController>(); Controller->Possess(Guard);
    Guard->SetNPCDefinition(Guard->GetNPCDefinition()); Controller->bRequireThreatMemoryForTargeting = true;
    auto* Brain = NewObject<USovLoadedEncounterTestBrain>(Controller);
    Controller->AddInstanceComponent(Brain); Brain->RegisterComponent(); Controller->BrainComponent = Brain;
    FString Error; FNarrativeActorRecord EntryRecord;
    // The authored pre-entry opt-in and story-cast opt-out must not weaken an already captured roster.
    F.Director->bHoldParticipantsBeforeEntry = false;
    F.Director->bHoldNonVictoryParticipantsBeforeEntry = false;
    if (!TestTrue(TEXT("Real entry capture freezes the complete roster"), F.Director->CaptureEntryCheckpoint(F.Player, Error))
        || !TestTrue(TEXT("Narrative writes the actual frozen-entry actor record"), Save->CreateActorRecord(F.Director, EntryRecord)))
    { AddError(Error); return false; }
    TestFalse(TEXT("Arena-entry record precedes the first attempt identity"), F.Director->GetAttemptId().IsValid());
    if (!TestTrue(TEXT("Ordinary begin releases transient entry leases"), F.Director->BeginEncounter())) { return false; }
    TestEqual(TEXT("Initial director Busy released"), GuardASC->GetTagCount(Busy), 0);
    TestFalse(TEXT("Initial brain resumed"), Brain->IsPaused());
    TestFalse(TEXT("Initial threat lease released"), Controller->IsThreatMemorySuspended());
    if (!TestTrue(TEXT("Narrative restores the serialized entry"), Save->LoadActorFromRecord(F.Director, EntryRecord))) { return false; }
    TestEqual(TEXT("Existing load policy requires an explicit retry"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
    TestFalse(TEXT("Loading does not invent an attempt"), F.Director->GetAttemptId().IsValid());
    TestTrue(TEXT("Failed loaded entry keeps its native refresh tick enabled"), F.Director->IsActorTickEnabled());
    TestTrue(TEXT("Live brain is frozen on load"), Brain->IsPaused());
    TestTrue(TEXT("Live threat owner is frozen on load"), Controller->IsThreatMemorySuspendedOnlyBy(F.Director));
    for (const auto& P : F.Director->Participants)
    {
        auto* ASC = P.Character->GetNarrativeAbilitySystemComponent();
        TestEqual(TEXT("Every loaded participant, including opted-out story cast, receives exactly one Busy"), ASC->GetTagCount(Busy), 1);
        TestEqual(TEXT("Every loaded participant receives exactly one protection contribution"), ASC->GetTagCount(Protection), 1);
    }
    GuardASC->AddLooseGameplayTag(Busy); GuardASC->AddLooseGameplayTag(Protection);
    FSovCrucibleRuntimeTestAccess::LoadedHoldStep(F.Director);
    TestEqual(TEXT("Reconciliation preserves independent Busy ownership"), GuardASC->GetTagCount(Busy), 2);
    TestEqual(TEXT("Reconciliation preserves independent protection ownership"), GuardASC->GetTagCount(Protection), 2);
    GuardASC->RemoveLooseGameplayTag(Busy); GuardASC->RemoveLooseGameplayTag(Protection);
    TestFalse(TEXT("A loaded failure cannot use ordinary Begin as a retry bypass"), F.Director->BeginEncounter());
    TestEqual(TEXT("Load and holding add no journal facts"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    auto* Observer = F.Observer(); F.Director->OnEncounterRestoreFailed.AddDynamic(Observer, &USovEncounterObjectiveTestObserver::RestoreFailed);
    if (!TestTrue(TEXT("Existing explicit retry starts its normal reconstruction"), F.Director->RetryEncounter(Error))) { AddError(Error); return false; }
    for (int32 Index = 0; Index < 8 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Index)
    { F.NextFrame(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
    if (!TestEqual(TEXT("Native reconstruction reaches Active"), F.Director->GetEncounterState(), ESovEncounterState::Active))
    { AddError(Observer->RestoreError); return false; }
    TestFalse(TEXT("Explicit retry retires the loaded-entry hold generation"), FSovCrucibleRuntimeTestAccess::HasLoadedHold(F.Director));
    auto* Replacement = F.Director->GetParticipant(TEXT("Formation.Guard"));
    TestTrue(TEXT("Retry owns an actual replacement actor"), Replacement != Guard);
    TestTrue(TEXT("Only explicit retry creates a real attempt"), F.Director->GetAttemptId().IsValid());
    FSovCrucibleRuntimeTestAccess::LoadedHoldStep(F.Director);
    TestEqual(TEXT("Retired loaded hold never freezes the active replacement"), Replacement->GetNarrativeAbilitySystemComponent()->GetTagCount(Busy), 0);
    TestEqual(TEXT("Retry creates no success facts"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLoadedEncounterLateReadinessTest,
    "ProjectVelkorran.Campaign.Encounter.LoadedEntryReconcilesLateReadinessAndCurrentOwners",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovLoadedEncounterLateReadinessTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(false, true);
    if (!TestNotNull(TEXT("Ready campaign"), F.ASC)) { return false; }
    const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
    const auto Protection = FNarrativeGameplayTags::Get().State_Invulnerable;
    auto* Guard = CastChecked<ASovWaveReleaseTestNPC>(F.Director->GetParticipant(TEXT("Formation.Guard")));
    auto* GuardASC = Guard->GetNarrativeAbilitySystemComponent();
    auto* Controller = F.World->SpawnActor<ANarrativeNPCController>(); Controller->Possess(Guard);
    Guard->SetNPCDefinition(Guard->GetNPCDefinition()); Controller->bRequireThreatMemoryForTargeting = true;
    auto* Brain = NewObject<USovLoadedEncounterTestBrain>(Controller);
    Controller->AddInstanceComponent(Brain); Brain->RegisterComponent(); Controller->BrainComponent = Brain;
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>(); FNarrativeActorRecord EntryRecord; FString Error;
    if (!TestTrue(TEXT("Native capture before combat"), F.Director->CaptureEntryCheckpoint(F.Player, Error))
        || !TestTrue(TEXT("Real entry serialization"), Save->CreateActorRecord(F.Director, EntryRecord))
        || !TestTrue(TEXT("Native begin releases entry-only leases"), F.Director->BeginEncounter())) { AddError(Error); return false; }
    Guard->SetSnapshotReadyForTest(false);
    if (!TestTrue(TEXT("Native load accepts the frozen record before this NPC is ready"), Save->LoadActorFromRecord(F.Director, EntryRecord))) { return false; }
    TestEqual(TEXT("Initialization is not blocked with an early Busy contribution"), GuardASC->GetTagCount(Busy), 0);
    TestEqual(TEXT("Initialization receives no early protection mutation"), GuardASC->GetTagCount(Protection), 0);
    TestTrue(TEXT("Already-created controller is frozen even while visual/GAS initialization continues"), Brain->IsPaused());
    TestTrue(TEXT("Early controller cannot acquire combat observations"), Controller->IsThreatMemorySuspendedOnlyBy(F.Director));
    Guard->SetNPCDefinition(Guard->GetNPCDefinition());
    TestTrue(TEXT("Fixture completes the exact existing NPC readiness"), Guard->IsEncounterSnapshotReady());
    FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f);
    TestEqual(TEXT("Native tick installs the late-ready actor's owned Busy"), GuardASC->GetTagCount(Busy), 1);
    TestEqual(TEXT("Native tick installs the late-ready actor's owned protection"), GuardASC->GetTagCount(Protection), 1);
    Brain->ResumeLogic(TEXT("Simulate late activity restoration completing on the same controller"));
    const int32 BeforeReconcile = Brain->PauseCount;
    FSovCrucibleRuntimeTestAccess::LoadedHoldStep(F.Director);
    TestTrue(TEXT("Late activity restart cannot leave loaded failure running"), Brain->IsPaused());
    TestEqual(TEXT("Only the resumed brain is paused again"), Brain->PauseCount, BeforeReconcile + 1);
    FSovCrucibleRuntimeTestAccess::LoadedHoldStep(F.Director);
    TestEqual(TEXT("Repeated hold does not duplicate brain pause"), Brain->PauseCount, BeforeReconcile + 1);
    TestEqual(TEXT("Repeated hold does not duplicate tags"), GuardASC->GetTagCount(Busy), 1);
    GuardASC->RemoveLooseGameplayTag(Busy); GuardASC->RemoveLooseGameplayTag(Protection);
    FSovCrucibleRuntimeTestAccess::LoadedHoldStep(F.Director);
    TestEqual(TEXT("Zero-count loose-tag reset is repaired on the same owned ASC"), GuardASC->GetTagCount(Busy), 1);
    TestEqual(TEXT("Owned protection is repaired after its zero-count reset"), GuardASC->GetTagCount(Protection), 1);
    auto* ReplacementController = F.World->SpawnActor<ANarrativeNPCController>(); ReplacementController->Possess(Guard);
    Guard->SetNPCDefinition(Guard->GetNPCDefinition()); ReplacementController->bRequireThreatMemoryForTargeting = true;
    auto* ReplacementBrain = NewObject<USovLoadedEncounterTestBrain>(ReplacementController);
    ReplacementController->AddInstanceComponent(ReplacementBrain); ReplacementBrain->RegisterComponent(); ReplacementController->BrainComponent = ReplacementBrain;
    FSovCrucibleRuntimeTestAccess::LoadedHoldStep(F.Director);
    TestTrue(TEXT("Same participant's current controller is held after replacement"), ReplacementController->IsThreatMemorySuspendedOnlyBy(F.Director));
    TestTrue(TEXT("Same participant's current brain is paused"), ReplacementBrain->IsPaused());
    TestFalse(TEXT("Current controller cannot admit hostile observations while awaiting retry"), ReplacementController->ReportThreatObservation(F.Player,
        ENarrativeThreatSource::Damage, F.Player->GetActorLocation(), 1.f, 1.f, 8.f));
    TestEqual(TEXT("Late readiness never auto-starts the encounter"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
    TestFalse(TEXT("Late readiness never invents an attempt identity"), F.Director->GetAttemptId().IsValid());
    TestEqual(TEXT("No proof is manufactured during readiness reconciliation"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLoadedEncounterHoldRetirementTest,
    "ProjectVelkorran.Campaign.Encounter.LoadedEntryHoldRetiresAcrossNativeCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovLoadedEncounterHoldRetirementTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F;
    if (!TestNotNull(TEXT("Ready campaign"), F.ASC)) { return false; }
    const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
    auto* GuardASC = F.Director->GetParticipant(TEXT("Formation.Guard"))->GetNarrativeAbilitySystemComponent();
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>(); FNarrativeActorRecord EntryRecord; FString Error;
    if (!TestTrue(TEXT("Native entry capture"), F.Director->CaptureEntryCheckpoint(F.Player, Error))
        || !TestTrue(TEXT("Native entry serialization"), Save->CreateActorRecord(F.Director, EntryRecord))
        || !TestTrue(TEXT("Native begin"), F.Director->BeginEncounter())) { AddError(Error); return false; }
    const FDelegateHandle Handle = GuardASC->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::AnyCountChange).AddLambda(
        [&](FGameplayTag, int32 Count) { if (Count == 1) { FSovCrucibleRuntimeTestAccess::RetireWaveContext(F.Director); } });
    TestTrue(TEXT("Load reaches the real tag callback"), Save->LoadActorFromRecord(F.Director, EntryRecord));
    GuardASC->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::AnyCountChange).Remove(Handle);
    auto* SurvivorASC = F.Director->GetParticipant(TEXT("Survivor.Dominion"))->GetNarrativeAbilitySystemComponent();
    TestEqual(TEXT("A retired generation cannot hold the next participant"), SurvivorASC->GetTagCount(Busy), 0);
    FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f);
    TestFalse(TEXT("The stale loaded-hold generation is retired on tick"), FSovCrucibleRuntimeTestAccess::HasLoadedHold(F.Director));
    FSovCrucibleRuntimeTestAccess::LoadedHoldStep(F.Director);
    TestEqual(TEXT("Later callbacks cannot rearm a retired load"), SurvivorASC->GetTagCount(Busy), 0);
    TestEqual(TEXT("Retirement does not publish campaign proof"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}


namespace
{
    ASovAurelionRequestActor* MakeEncounterRetryRequest(FEncounterObjectiveWorld& F)
    {
        auto* Terminal = F.World->SpawnActor<ASovAurelionRequestActor>();
        if (!Terminal) { return nullptr; }
        Terminal->RequestId = TEXT("Test.Entry.Retry"); Terminal->MissionId = F.Mission->MissionId;
        Terminal->BeatId = F.Objective->CompletionBeat; Terminal->Operation = ESovAurelionRequest::RetryEncounter;
        Terminal->RetryObjective = F.Objective; Terminal->RetryDirector = F.Director;
        Terminal->SetActorLocation(F.Player->GetActorLocation() + FVector(180, 0, 0));
        return Terminal;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionExplicitRetryHoldTest,
    "ProjectVelkorran.Campaign.Aurelion.Request.LoadedEntryNativeHoldRestoresFreshAttempt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionExplicitRetryHoldTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Native hold settings owner"), GEngine)) { return false; }
    TStrongObjectPtr<UGameUserSettings> PreviousSettings(GEngine->GameUserSettings);
    TStrongObjectPtr<UNarrativeGameUserSettings> HoldSettings(NewObject<UNarrativeGameUserSettings>());
    TGuardValue<TObjectPtr<UGameUserSettings>> ScopedSettings(GEngine->GameUserSettings, HoldSettings.Get());
    FEncounterObjectiveWorld F(false, false, true);
    if (!TestNotNull(TEXT("Ready Aurelion campaign player"), F.ASC)) { return false; }
    FString Error; FText UseError; FNarrativeActorRecord EntryRecord;
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
    if (!TestNotNull(TEXT("Actual Narrative save owner"), Save)
        || !TestTrue(TEXT("Capture the actual inactive entry before its first attempt"), F.Director->CaptureEntryCheckpoint(F.Player, Error))
        || !TestTrue(TEXT("Serialize the native zero-attempt checkpoint"), Save->CreateActorRecord(F.Director, EntryRecord))
        || !TestTrue(TEXT("Initial ordinary objective starts combat"), F.Start())) { AddError(Error + F.SetupError); return false; }
    const FGuid OriginalAttempt = F.Director->GetAttemptId();
    if (!TestTrue(TEXT("Native save restore returns to the captured entry"), Save->LoadActorFromRecord(F.Director, EntryRecord))) { return false; }
    TestEqual(TEXT("Loaded entry awaits an explicit request"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
    TestFalse(TEXT("An inactive entry record has no fabricated attempt"), F.Director->GetAttemptId().IsValid());
    TestFalse(TEXT("No overlap startup is required, including E4B-style entries"), F.Objective->bStartOnPlayerOverlap);
    auto* Terminal = MakeEncounterRetryRequest(F);
    if (!TestNotNull(TEXT("Physical retry surface"), Terminal)
        || !TestTrue(TEXT("Loaded entry offers a retry"), Terminal->CanUse(F.Player, UseError))) { AddError(UseError.ToString()); return false; }
    auto* Interaction = NewObject<USovCampaignTerminalTestInteraction>(F.PC);
    F.PC->AddInstanceComponent(Interaction); Interaction->RegisterComponent(); Interaction->Configure(F.PC);
    Interaction->Activate(); Interaction->SetViewedInteractable(Terminal->Interactable);
    TestFalse(TEXT("Use ordinary holding"), HoldSettings->UseTapInteractions());
    TestEqual(TEXT("Native action text is explicit"), Terminal->Interactable->GetInteractableActionText(F.Player, Interaction).ToString(), FString(TEXT("Retry encounter")));
    Terminal->Tick(.15f);
    TestEqual(TEXT("World label names the available action"), Terminal->Label->Text.ToString(), FString(TEXT("Retry encounter")));
    TestTrue(TEXT("Available retry label is visible"), Terminal->Label->IsVisible());
    const uint64 LoadedGeneration = F.Director->GetLifecycleGeneration();
    auto* PreviousGuard = F.Director->GetParticipant(TEXT("Formation.Guard"));
    Interaction->BeginInteract(); Interaction->TickComponent(.2f, LEVELTICK_All, nullptr); Interaction->EndInteract(); F.NextFrame();
    TestFalse(TEXT("Early release reserves no retry"), Terminal->IsRequestPending());
    TestEqual(TEXT("Early release retains the failed generation"), F.Director->GetLifecycleGeneration(), LoadedGeneration);
    Interaction->BeginInteract(); Interaction->TickComponent(.36f, LEVELTICK_All, nullptr); Interaction->EndInteract();
    TestTrue(TEXT("The completed Narrative hold queues one retry"), Terminal->IsRequestPending());
    TestFalse(TEXT("A duplicate request cannot queue during the same hold"), Terminal->RequestUse(F.Player, UseError));
    F.NextFrame();
    TestEqual(TEXT("Only the native objective requests restoration"), F.Director->GetEncounterState(), ESovEncounterState::Restoring);
    TestEqual(TEXT("Exactly one restore generation was admitted"), F.Director->GetLifecycleGeneration(), LoadedGeneration + 1);
    TestFalse(TEXT("Input request retires after native dispatch"), Terminal->IsRequestPending());
    TestFalse(TEXT("A second retry is unavailable while restoring"), Terminal->CanUse(F.Player, UseError));
    auto* Observer = F.Observer(); F.Director->OnEncounterRestoreFailed.AddDynamic(Observer, &USovEncounterObjectiveTestObserver::RestoreFailed);
    for (int32 Step = 0; Step < 8 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Step)
    { F.NextFrame(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
    if (!TestEqual(TEXT("Actual NPC reconstruction reaches Active"), F.Director->GetEncounterState(), ESovEncounterState::Active))
    { AddError(Observer->RestoreError); return false; }
    TestTrue(TEXT("The director owns a new valid attempt"), F.Director->GetAttemptId().IsValid() && F.Director->GetAttemptId() != OriginalAttempt);
    TestTrue(TEXT("The checkpoint reconstructed the actual guard"), F.Director->GetParticipant(TEXT("Formation.Guard")) != PreviousGuard);
    TestFalse(TEXT("The retired guard cannot retain gameplay ownership"), IsValid(PreviousGuard));
    TestEqual(TEXT("Retry input supplies no victory or journal proof"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    Terminal->Tick(.15f); TestFalse(TEXT("Retry prompt hides during active combat"), Terminal->Label->IsVisible());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionExplicitRetryRetirementTest,
    "ProjectVelkorran.Campaign.Aurelion.Request.RetryRetiresChangedCheckpointAndInputOwners",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionExplicitRetryRetirementTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 4; ++Case)
    {
        FEncounterObjectiveWorld F(false, false, true);
        if (!TestNotNull(TEXT("Ready retry context"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
        F.Director->FailEncounter();
        const FGuid FailedAttempt = F.Director->GetAttemptId();
        TestTrue(TEXT("A failed live attempt retains its real identity"), FailedAttempt.IsValid());
        auto* Guard = F.Director->GetParticipant(TEXT("Formation.Guard"));
        auto* Terminal = MakeEncounterRetryRequest(F); FText Error;
        if (!TestTrue(TEXT("The failed attempt queues ordinary retry admission"), Terminal->RequestUse(F.Player, Error))) { AddError(Error.ToString()); return false; }
        if (Case == 0) { F.ASC->SetCharacterReadyEpoch(F.ASC->GetCharacterReadyEpoch() + 1); }
        else if (Case == 1) { F.Director->Load_Implementation(); }
        else if (Case == 2) { Terminal->RetryObjective = nullptr; }
        else { Terminal->Interactable->Deactivate(); }
        const uint64 RetiredGeneration = F.Director->GetLifecycleGeneration();
        const FGuid RetiredAttempt = F.Director->GetAttemptId();
        F.NextFrame();
        TestFalse(TEXT("Stale input slot is retired"), Terminal->IsRequestPending());
        TestEqual(TEXT("Stale input never advances the new checkpoint generation"), F.Director->GetLifecycleGeneration(), RetiredGeneration);
        TestEqual(TEXT("Stale input never changes the current attempt"), F.Director->GetAttemptId(), RetiredAttempt);
        TestEqual(TEXT("Stale input leaves Failed until a new request"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
        TestEqual(TEXT("Stale input cannot destroy the current roster"), F.Director->GetParticipant(TEXT("Formation.Guard")), Guard);
        TestEqual(TEXT("Stale input cannot fabricate campaign progress"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionExplicitRetryAdmissionTest,
    "ProjectVelkorran.Campaign.Aurelion.Request.RetryRejectsFreshAmbiguousAndObstructedEntries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionExplicitRetryAdmissionTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(false, false, true);
    if (!TestNotNull(TEXT("Ready retry admission context"), F.ASC)) { return false; }
    auto* Terminal = MakeEncounterRetryRequest(F); FText Error;
    TestFalse(TEXT("Retry cannot start fresh uncheckpointed combat"), Terminal->CanUse(F.Player, Error));
    if (!F.Start()) { AddError(F.SetupError); return false; }
    TestFalse(TEXT("Retry interaction is unavailable for active combat"), Terminal->CanUse(F.Player, Error));
    F.Director->FailEncounter();
    TestTrue(TEXT("Exact failed entry permits an explicit request"), Terminal->CanUse(F.Player, Error));
    Terminal->SetActorLocation(F.Player->GetActorLocation() + FVector(650, 0, 0));
    TestFalse(TEXT("A distant retry is not an input shortcut"), Terminal->CanUse(F.Player, Error));
    Terminal->SetActorLocation(F.Player->GetActorLocation() + FVector(180, 0, 0));
    auto* Blocker = F.World->SpawnActor<AActor>(); auto* Shape = NewObject<UBoxComponent>(Blocker);
    Blocker->SetRootComponent(Shape); Blocker->AddInstanceComponent(Shape); Shape->SetBoxExtent(FVector(15, 150, 200));
    Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Shape->SetCollisionResponseToAllChannels(ECR_Block);
    Shape->RegisterComponent(); Blocker->SetActorLocation(F.Player->GetActorLocation() + FVector(90, 0, 0));
    TestFalse(TEXT("Real opaque geometry blocks retry"), Terminal->CanUse(F.Player, Error)); Blocker->Destroy();
    auto* OtherObjective = F.World->SpawnActor<ASovCampaignEncounterObjective>();
    OtherObjective->MissionId = F.Objective->MissionId; OtherObjective->CompletionBeat = F.Objective->CompletionBeat;
    OtherObjective->EncounterDirector = F.Director;
    TestFalse(TEXT("A duplicate objective cannot select checkpoint ownership"), Terminal->CanUse(F.Player, Error)); OtherObjective->Destroy();
    auto* OtherDirector = F.World->SpawnActor<ASovEncounterDirector>(); OtherDirector->EncounterId = F.Director->EncounterId;
    TestFalse(TEXT("A duplicate director identity is rejected"), Terminal->CanUse(F.Player, Error)); OtherDirector->Destroy();
    Terminal->BeatId = F.Mission->Beats[1].BeatId;
    TestFalse(TEXT("A future objective cannot retry another beat's checkpoint"), Terminal->CanUse(F.Player, Error)); Terminal->BeatId = F.Objective->CompletionBeat;
    const FGameplayTag RequiredProtagonist = F.Mission->Beats[0].RequiredProtagonist;
    F.Mission->Beats[0].RequiredProtagonist = FSovGameplayTags::Get().Character_Player_Selene;
    TestFalse(TEXT("Tarrik cannot invoke a Selene entry"), Terminal->CanUse(F.Player, Error)); F.Mission->Beats[0].RequiredProtagonist = RequiredProtagonist;
    TestTrue(TEXT("Restoring the exact current binding restores admission"), Terminal->CanUse(F.Player, Error));
    TestEqual(TEXT("Admission checks alone neither replace actors nor write proof"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterStoryTagsRoundTripTest,
    "ProjectVelkorran.Campaign.EncounterObjective.ProtectedStoryTagsSurviveSavedEntryRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterStoryTagsRoundTripTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(false, false, false, true);
    if (!TestNotNull(TEXT("Ready story-roster campaign"), F.ASC)) { return false; }
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
    if (!TestNotNull(TEXT("Actual Narrative save owner"), Save)) { return false; }
    const FName ParticipantIds[] = {TEXT("Survivor.Dominion"), TEXT("Survivor.Reformation")};
    const FName StoryTags[] = {TEXT("Aurelion_TrappedMarine"), TEXT("Aurelion_Lyric")};
    ASovNPCCharacterBase* Original[2] = {};
    FNarrativeActorRecord OriginalRecords[2];
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Original[Index] = F.Director->GetParticipant(ParticipantIds[Index]);
        if (!TestNotNull(TEXT("Protected story participant"), Original[Index])) { return false; }
        // The shared class has no semantic identity; each placement supplies its own.
        Original[Index]->Tags.Add(StoryTags[Index]);
        Original[Index]->Tags.Add(TEXT("External.Story.Marker"));
        // A later constructor/default tag must survive even when absent from the snapshot.
        Original[Index]->Tags.Remove(TEXT("Test.Story.Default"));
        if (!TestTrue(TEXT("Capture each original native GUID"), Save->CreateActorRecord(Original[Index], OriginalRecords[Index]))) { return false; }
    }
    FString Error; FNarrativeActorRecord SavedEntry;
    if (!TestTrue(TEXT("Capture the native protected entry"), F.Director->CaptureEntryCheckpoint(F.Player, Error))
        || !TestTrue(TEXT("Serialize the real director and nested NPC records"), Save->CreateActorRecord(F.Director, SavedEntry))
        || !F.Start()) { AddError(Error + F.SetupError); return false; }
    F.Kill(ParticipantIds[0]);
    TestEqual(TEXT("Actual protected death fails the encounter"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
    // Erase only test-side backing values to prove the real SaveGame archive restores them.
    FSovCrucibleRuntimeTestAccess::ClearEntryActorTags(F.Director);
    if (!TestTrue(TEXT("Load the exact serialized native entry"), Save->LoadActorFromRecord(F.Director, SavedEntry))) { return false; }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TestTrue(TEXT("Nested SaveGame record restored its original story identity"),
            FSovCrucibleRuntimeTestAccess::EntryHasActorTag(F.Director, ParticipantIds[Index], StoryTags[Index]));
    }
    auto* Observer = F.Observer();
    F.Director->OnEncounterRestoreFailed.AddDynamic(Observer, &USovEncounterObjectiveTestObserver::RestoreFailed);
    if (!TestTrue(TEXT("The ordinary objective invokes native retry"), F.Objective->StartEncounter(F.Player, Error))) { AddError(Error); return false; }
    for (int32 Step = 0; Step < 8 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Step)
    { F.NextFrame(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
    if (!TestEqual(TEXT("Restored roster is released by the native director"), F.Director->GetEncounterState(), ESovEncounterState::Active))
    { AddError(Observer->RestoreError); return false; }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Restored = Cast<ASovEncounterStoryIdentityNPC>(F.Director->GetParticipant(ParticipantIds[Index]));
        if (!TestNotNull(TEXT("Replacement retains its authored class"), Restored)) { return false; }
        TestTrue(TEXT("Retry created a new participant and retired the previous object"), Restored != Original[Index] && !IsValid(Original[Index]));
        TestTrue(TEXT("Semantic identity exists before normal actor initialization callbacks"), Restored->bHadStoryIdentityAtInitialization);
        TestTrue(TEXT("Constructor/default and original external tags both survive"),
            Restored->ActorHasTag(TEXT("Test.Story.Default")) && Restored->ActorHasTag(TEXT("External.Story.Marker")));
        TestFalse(TEXT("A shared class never acquires its other placement's story identity"), Restored->ActorHasTag(StoryTags[1-Index]));
        TArray<AActor*> Matches; UGameplayStatics::GetAllActorsWithTag(F.World, StoryTags[Index], Matches);
        TestEqual(TEXT("Tag-based cinematic lookup sees exactly one actor"), Matches.Num(), 1);
        if (Matches.Num() == 1) { TestTrue(TEXT("The tagged actor is the registered native replacement"), Matches[0] == Restored); }
        FNarrativeActorRecord RestoredRecord;
        if (!TestTrue(TEXT("Replacement remains saveable through Narrative"), Save->CreateActorRecord(Restored, RestoredRecord))) { return false; }
        TestEqual(TEXT("Narrative stable GUID is unchanged by tag restoration"), RestoredRecord.ActorGUID, OriginalRecords[Index].ActorGUID);
    }
    // A later generic world load also rebinds dynamic replacements by saved GUID.
    // Remove only the test-supplied instance identity to model its omission from Actor.ByteData.
    ASovNPCCharacterBase* Rebound[2] = {};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Rebound[Index] = F.Director->GetParticipant(ParticipantIds[Index]);
        Rebound[Index]->Tags.Remove(StoryTags[Index]);
        Rebound[Index]->Tags.Add(TEXT("External.Rebound.New"));
    }
    // A different live director must not have its actor's metadata changed during reconciliation.
    auto* Foreign = F.World->SpawnActor<ASovEncounterDirector>();
    if (!TestNotNull(TEXT("Independent encounter owner"), Foreign)) { return false; }
    FSovEncounterParticipant ForeignParticipant; ForeignParticipant.ParticipantId = TEXT("External.Participant");
    ForeignParticipant.Character = Rebound[0]; Foreign->Participants.Add(ForeignParticipant);
    if (!TestTrue(TEXT("Actual Narrative load rebinds previously replaced actors"), Save->LoadActorFromRecord(F.Director, SavedEntry))) { return false; }
    TestFalse(TEXT("A live foreign owner vetoes metadata merging"), Rebound[0]->ActorHasTag(StoryTags[0]));
    TestTrue(TEXT("The uniquely owned GUID match recovers its semantic identity"), Rebound[1]->ActorHasTag(StoryTags[1]));
    Foreign->Participants.Reset(); Foreign->Destroy();
    if (!TestTrue(TEXT("A later exact-owner reconciliation restores the remaining binding"), Save->LoadActorFromRecord(F.Director, SavedEntry))) { return false; }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TestTrue(TEXT("World-load reconciliation retains the existing exact GUID object"), F.Director->GetParticipant(ParticipantIds[Index]) == Rebound[Index]);
        TestTrue(TEXT("Existing constructor and newly added external tags remain intact"),
            Rebound[Index]->ActorHasTag(TEXT("Test.Story.Default")) && Rebound[Index]->ActorHasTag(TEXT("External.Rebound.New")));
        TestTrue(TEXT("Generic reload restores the captured story tag without another retry"), Rebound[Index]->ActorHasTag(StoryTags[Index]));
    }
    TestEqual(TEXT("Identity restoration grants no cinematic or encounter receipt"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterEmptyTagSnapshotTest,
    "ProjectVelkorran.Campaign.EncounterObjective.EmptyLegacyTagSnapshotPreservesNewDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEncounterEmptyTagSnapshotTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(false, false, false, true);
    if (!TestNotNull(TEXT("Ready legacy-empty snapshot fixture"), F.ASC)) { return false; }
    FString Error; FNarrativeActorRecord EmptyTagEntry;
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
    if (!TestNotNull(TEXT("Actual Narrative save owner"), Save)
        || !TestTrue(TEXT("Capture the real native entry"), F.Director->CaptureEntryCheckpoint(F.Player, Error))) { AddError(Error); return false; }
    FSovCrucibleRuntimeTestAccess::ClearEntryActorTags(F.Director);
    if (!TestTrue(TEXT("Save the empty-tag default used by pre-field records"), Save->CreateActorRecord(F.Director, EmptyTagEntry))
        || !F.Start() || !TestTrue(TEXT("Load the empty-tag entry"), Save->LoadActorFromRecord(F.Director, EmptyTagEntry))) { AddError(F.SetupError); return false; }
    auto* Unrelated = F.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Independent world actor"), Unrelated)) { return false; }
    Unrelated->Tags = {TEXT("External.World.Owner")};
    const TArray<FName> UnrelatedTags = Unrelated->Tags;
    if (!TestTrue(TEXT("Explicit native retry admits the backward-readable entry"), F.Objective->StartEncounter(F.Player, Error))) { AddError(Error); return false; }
    for (int32 Step = 0; Step < 8 && F.Director->GetEncounterState() == ESovEncounterState::Restoring; ++Step)
    { F.NextFrame(); FSovCrucibleRuntimeTestAccess::Step(F.Director, .016f); }
    if (!TestEqual(TEXT("Empty tag data does not prevent ordinary reconstruction"), F.Director->GetEncounterState(), ESovEncounterState::Active)) { return false; }
    for (const FName Id : {FName(TEXT("Survivor.Dominion")), FName(TEXT("Survivor.Reformation"))})
    {
        auto* Restored = Cast<ASovEncounterStoryIdentityNPC>(F.Director->GetParticipant(Id));
        if (!TestNotNull(TEXT("Reconstructed protected role"), Restored)) { return false; }
        TestTrue(TEXT("Missing legacy tags never erase newly authored defaults"), Restored->ActorHasTag(TEXT("Test.Story.Default")));
        TestFalse(TEXT("An absent saved semantic identity is not guessed"), Restored->bHadStoryIdentityAtInitialization);
    }
    TestTrue(TEXT("An unrelated actor's tags remain untouched"), Unrelated->Tags == UnrelatedTags);
    TestEqual(TEXT("Backward-readable metadata supplies no durable proof"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThermalTransferredProtectionLoadTest,
    "ProjectVelkorran.Campaign.Aurelion.TransferredProtectionRestoresFromActualEntryRecordWithoutGrantingProof",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThermalTransferredProtectionLoadTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true);
    if (!TestNotNull(TEXT("Ready native phase fixture"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    auto* PhaseA = CastChecked<ASovAurelionLinkPhaseDirector>(F.Director);
    for (const auto& Binding : PhaseA->RequiredLinks)
    {
        auto* Link = PhaseA->GetParticipant(Binding.ParticipantId)->FindComponentByClass<USovCommandLinkComponent>();
        FSovCommandLinkSeverResult Result;
        if (!TestEqual(TEXT("Actual native link sever earns the transfer boundary"),
            Link->TrySeverCommandLink(F.Player, Result), ESovCommandLinkSeverResolution::NewlySevered)) { return false; }
    }
    FSovCrucibleRuntimeTestAccess::Step(PhaseA, .016f); F.NextFrame();
    if (!TestEqual(TEXT("Real link phase completed"), PhaseA->GetEncounterState(), ESovEncounterState::Succeeded)) { return false; }
    auto* Original = F.World->SpawnActor<ASovAurelionThermalPhaseDirector>();
    if (!TestNotNull(TEXT("Initially empty thermal destination"), Original)) { return false; }
    Original->EncounterId = TEXT("Test.ProtectionLoadB"); Original->EliteParticipantId = PhaseA->EliteParticipantId;
    FString Error;
    if (!TestTrue(TEXT("Native completed phase transfers protected actors"), FSovCrucibleRuntimeTestAccess::Transfer(PhaseA, Original, Error))
        || !TestTrue(TEXT("Native phase-B entry captures the transferred protected membership"), Original->CaptureEntryCheckpoint(F.Player, Error)))
    { AddError(Error); return false; }
    const TSet<FName> Expected = Original->ProtectedParticipantIds;
    if (!TestEqual(TEXT("Both transferred protected roles were actually captured"), Expected.Num(), 2)) { return false; }
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>(); FNarrativeActorRecord Record;
    if (!TestTrue(TEXT("Actual Narrative record captures old-schema saved entry metadata"), Save->CreateActorRecord(Original, Record))) { return false; }
    const FName EncounterId = Original->EncounterId, EliteId = Original->EliteParticipantId;
    Original->Destroy();
    auto* Loaded = F.World->SpawnActor<ASovAurelionThermalPhaseDirector>();
    if (!TestNotNull(TEXT("Fresh native destination has no runtime transfer arrays"), Loaded)) { return false; }
    Loaded->EncounterId = EncounterId; Loaded->EliteParticipantId = EliteId;
    TestTrue(TEXT("Fresh destination protection starts empty"), Loaded->ProtectedParticipantIds.IsEmpty());
    if (!TestTrue(TEXT("Real Narrative deserialization/rebinding loads the entry"), Save->LoadActorFromRecord(Loaded, Record))) { return false; }
    TestEqual(TEXT("Loaded entry still requires ordinary explicit retry"), Loaded->GetEncounterState(), ESovEncounterState::Failed);
    TestEqual(TEXT("All saved protected roles are restored"), Loaded->ProtectedParticipantIds.Num(), Expected.Num());
    for (const FName Id : Expected)
    {
        TestTrue(TEXT("Exact saved protected identity restored"), Loaded->ProtectedParticipantIds.Contains(Id));
        TestNotNull(TEXT("Protection refers to the actual rebound NPC"), Loaded->GetParticipant(Id));
    }
    TestTrue(TEXT("Existing native protection validation accepts restored roles"), FSovCrucibleRuntimeTestAccess::ProtectionValid(Loaded));
    TestFalse(TEXT("Metadata restoration cannot create unearned fracture/victory proof"), Loaded->HasConfirmedVictory());
    const int32 JournalCount = F.PC->GetCampaignState()->GetJournal().Num();
    Loaded->ProtectedParticipantIds = { TEXT("External.ChangedProtection") };
    TestTrue(TEXT("Same saved record remains readable with an explicit conflicting configuration"), Save->LoadActorFromRecord(Loaded, Record));
    TestTrue(TEXT("Nonempty conflicting configuration is never silently replaced"), Loaded->ProtectedParticipantIds.Contains(TEXT("External.ChangedProtection")));
    TestFalse(TEXT("Original configuration mismatch fence remains enforced"), FSovCrucibleRuntimeTestAccess::ProtectionValid(Loaded));
    Loaded->ProtectedParticipantIds.Reset();
    TestTrue(TEXT("A fresh empty destination can reload the same legitimate metadata"), Save->LoadActorFromRecord(Loaded, Record));
    FSovCrucibleRuntimeTestAccess::CorruptSavedProtectionRole(Loaded, TEXT("Survivor.Dominion"));
    FNarrativeActorRecord Malformed;
    if (!TestTrue(TEXT("Fixture serializes a malformed saved protected role"), Save->CreateActorRecord(Loaded, Malformed))) { return false; }
    Loaded->ProtectedParticipantIds.Reset();
    TestTrue(TEXT("Narrative replays the malformed fixture for native refusal"), Save->LoadActorFromRecord(Loaded, Malformed));
    TestTrue(TEXT("No partial protection set publishes from malformed role data"), Loaded->ProtectedParticipantIds.IsEmpty());
    TestFalse(TEXT("Malformed protection cannot confirm native victory"), Loaded->HasConfirmedVictory());
    TestEqual(TEXT("Loading metadata adds no campaign receipt"), F.PC->GetCampaignState()->GetJournal().Num(), JournalCount);
    TestTrue(TEXT("Phase A remains relinquished throughout destination loads"), PhaseA->Participants.IsEmpty());
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDefeatedParticipantMetadataLoadTest,
    "ProjectVelkorran.Campaign.EncounterObjective.DefeatedMembershipSurvivesNativeTombstoneAndFreshDirectorLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDefeatedParticipantMetadataLoadTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F;
    if (!TestNotNull(TEXT("Ready native campaign fixture"), F.ASC)) { return false; }
    const FName GuardId(TEXT("Formation.Guard"));
    // Eligibility is saved metadata; this actor is never promoted to a Mass entity.
    F.Director->Participants[0].bAllowMassRepresentation = true;
    if (!F.Start()) { AddError(F.SetupError); return false; }
    auto* Guard = F.Director->GetParticipant(GuardId);
    F.Kill(GuardId); F.NextFrame();
    if (!TestTrue(TEXT("A real native death receipt confirms completed victory"), F.Director->HasConfirmedVictory())) { return false; }
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
    FNarrativeActorRecord CorpseRecord, DirectorRecord;
    if (!TestNotNull(TEXT("Actual Narrative record owner"), Save)
        || !TestTrue(TEXT("Actual corpse is captured"), Save->CreateActorRecord(Guard, CorpseRecord))
        || !TestTrue(TEXT("Actual terminal NPC record is a tombstone"), CorpseRecord.bDestroyed)
        || !TestTrue(TEXT("Actual completed director is serialized"), Save->CreateActorRecord(F.Director, DirectorRecord))) { return false; }
    const FGuid Attempt = F.Director->GetAttemptId();
    const FName EncounterId = F.Director->EncounterId;
    const auto ExpectedEntry = FSovCrucibleRuntimeTestAccess::SavedEntry(F.Director);
    const auto ExpectedDefeats = FSovCrucibleRuntimeTestAccess::SavedDefeats(F.Director);
    const auto ExpectedProtection = F.Director->ProtectedParticipantIds;
    const int32 JournalCount = F.PC->GetCampaignState()->GetJournal().Num();
    if (!TestTrue(TEXT("Native tombstone application removes the actual corpse"), Save->LoadActorFromRecord(Guard, CorpseRecord))) { return false; }
    TestFalse(TEXT("The old corpse is retired"), IsValid(Guard));
    TestNull(TEXT("No corpse remains in the native GUID cache"), Save->LookupActorByGUID(CorpseRecord.ActorGUID));
    F.Director->Destroy();
    auto* Loaded = F.World->SpawnActor<ASovEncounterDirector>();
    if (!TestNotNull(TEXT("Fresh director with no runtime registrations"), Loaded)) { return false; }
    Loaded->EncounterId = EncounterId; Loaded->ProtectedParticipantIds = ExpectedProtection;
    TestTrue(TEXT("Fresh runtime membership starts empty"), Loaded->Participants.IsEmpty());
    TArray<AActor*> BeforeNPCs; UGameplayStatics::GetAllActorsOfClass(F.World, ASovNPCCharacterBase::StaticClass(), BeforeNPCs);
    if (!TestTrue(TEXT("Actual Narrative deserialization loads the completed entry"), Save->LoadActorFromRecord(Loaded, DirectorRecord))) { return false; }
    TestEqual(TEXT("Exact saved attempt is retained"), Loaded->GetAttemptId(), Attempt);
    TestEqual(TEXT("Saved completed state is retained"), Loaded->GetEncounterState(), ESovEncounterState::Succeeded);
    TestEqual(TEXT("Complete saved membership returns without respawning a corpse"), Loaded->Participants.Num(), ExpectedEntry.Num());
    for (const auto& Expected : ExpectedEntry)
    {
        const auto* Actual = Loaded->Participants.FindByPredicate([&Expected](const auto& P) { return P.ParticipantId == Expected.ParticipantId; });
        if (!TestNotNull(TEXT("Each exact saved participant ID exists"), Actual)) { return false; }
        TestEqual(TEXT("Victory role remains exact"), Actual->bRequiredForVictory, Expected.bRequiredForVictory);
        TestEqual(TEXT("Mass eligibility remains exact without representation"), Actual->bAllowMassRepresentation, Expected.bAllowMassRepresentation);
        if (Expected.ParticipantId == GuardId) { TestNull(TEXT("Defeated membership owns no live actor"), Actual->Character.Get()); }
        else { TestNotNull(TEXT("Living protection keeps the real rebound actor"), Actual->Character.Get()); }
    }
    TestTrue(TEXT("Existing typed victory is preserved, not recreated"), Loaded->HasConfirmedVictory());
    TestEqual(TEXT("Native saved defeat count is unchanged"), FSovCrucibleRuntimeTestAccess::SavedDefeats(Loaded).Num(), ExpectedDefeats.Num());
    for (const FName Id : ExpectedDefeats) { TestTrue(TEXT("Exact saved defeat identity remains"), FSovCrucibleRuntimeTestAccess::SavedDefeats(Loaded).Contains(Id)); }
    TestTrue(TEXT("Same serialized load is idempotent"), Save->LoadActorFromRecord(Loaded, DirectorRecord));
    TestEqual(TEXT("A repeated load cannot append duplicate rows"), Loaded->Participants.Num(), ExpectedEntry.Num());
    TArray<AActor*> AfterNPCs; UGameplayStatics::GetAllActorsOfClass(F.World, ASovNPCCharacterBase::StaticClass(), AfterNPCs);
    TestEqual(TEXT("Metadata restoration never spawns another NPC"), AfterNPCs.Num(), BeforeNPCs.Num());
    TestEqual(TEXT("Loading membership grants no campaign receipt"), F.PC->GetCampaignState()->GetJournal().Num(), JournalCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTransferredDefeatMetadataLoadTest,
    "ProjectVelkorran.Campaign.Aurelion.TransferredDefeatLoadPreservesMissingLivingRefusalAndSourceRelinquishment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTransferredDefeatMetadataLoadTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true);
    if (!TestNotNull(TEXT("Ready actual link phase"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    auto* PhaseA = CastChecked<ASovAurelionLinkPhaseDirector>(F.Director);
    for (const auto& Binding : PhaseA->RequiredLinks)
    {
        auto* Link = PhaseA->GetParticipant(Binding.ParticipantId)->FindComponentByClass<USovCommandLinkComponent>();
        FSovCommandLinkSeverResult Result;
        if (!TestEqual(TEXT("Real native link sever earns the transfer"), Link->TrySeverCommandLink(F.Player, Result), ESovCommandLinkSeverResolution::NewlySevered)) { return false; }
    }
    FSovCrucibleRuntimeTestAccess::Step(PhaseA, .016f); F.NextFrame();
    auto* Original = F.World->SpawnActor<ASovAurelionThermalPhaseDirector>();
    if (!TestNotNull(TEXT("Fresh empty thermal destination"), Original)) { return false; }
    Original->EncounterId = TEXT("Test.DefeatedMetadataB"); Original->EliteParticipantId = PhaseA->EliteParticipantId;
    FString Error;
    if (!TestTrue(TEXT("Native phase ownership transfers the exact existing actors"), FSovCrucibleRuntimeTestAccess::Transfer(PhaseA, Original, Error))
        || !TestTrue(TEXT("Actual destination entry is captured"), Original->CaptureEntryCheckpoint(F.Player, Error))
        || !TestTrue(TEXT("Native destination attempt begins"), Original->BeginEncounter())) { AddError(Error); return false; }
    F.Director = Original;
    const FName DefeatedIds[] = {TEXT("Crucible.NodeWest"), TEXT("Crucible.NodeEast")};
    auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
    if (!TestNotNull(TEXT("Actual Narrative save owner"), Save)) { return false; }
    for (const FName Id : DefeatedIds)
    {
        auto* NPC = Original->GetParticipant(Id); F.Kill(Id);
        if (!TestTrue(TEXT("Native director recorded this actual death"), Original->HasConfirmedParticipantDefeat(Id))) { return false; }
        FNarrativeActorRecord Tombstone;
        if (!TestTrue(TEXT("Dead transferred actor is serialized"), Save->CreateActorRecord(NPC, Tombstone))
            || !TestTrue(TEXT("Transferred dead actor retains its terminal record"), Tombstone.bDestroyed)
            || !TestTrue(TEXT("Native tombstone application removes the dead transfer"), Save->LoadActorFromRecord(NPC, Tombstone))) { return false; }
        TestNull(TEXT("Dead transferred actor remains absent from native lookup"), Save->LookupActorByGUID(Tombstone.ActorGUID));
    }
    if (!TestTrue(TEXT("The surviving elite permits an ordinary failed boundary"), Original->FailEncounter())) { return false; }
    FNarrativeActorRecord SavedB, SavedA;
    if (!TestTrue(TEXT("Real phase-B state is serialized"), Save->CreateActorRecord(Original, SavedB))
        || !TestTrue(TEXT("Relinquished phase-A state is serialized"), Save->CreateActorRecord(PhaseA, SavedA))) { return false; }
    const FName EncounterId = Original->EncounterId, EliteId = Original->EliteParticipantId;
    const FGuid Attempt = Original->GetAttemptId();
    const int32 JournalCount = F.PC->GetCampaignState()->GetJournal().Num();
    const int32 DefeatCount = FSovCrucibleRuntimeTestAccess::SavedDefeats(Original).Num();
    auto* MissingLivingElite = Original->GetParticipant(EliteId);
    TestTrue(TEXT("The subsequently missing elite was still alive and unconfirmed"), MissingLivingElite->IsAlive()
        && !FSovCrucibleRuntimeTestAccess::SavedDefeats(Original).Contains(EliteId));
    MissingLivingElite->Destroy(); Original->Destroy();
    auto* Loaded = F.World->SpawnActor<ASovAurelionThermalPhaseDirector>();
    if (!TestNotNull(TEXT("New thermal actor has the same authored class"), Loaded)) { return false; }
    Loaded->EncounterId = EncounterId; Loaded->EliteParticipantId = EliteId;
    if (!TestTrue(TEXT("Native load reconstructs only admitted saved membership"), Save->LoadActorFromRecord(Loaded, SavedB))) { return false; }
    TestEqual(TEXT("The exact real failed attempt survives"), Loaded->GetAttemptId(), Attempt);
    TestEqual(TEXT("Four valid rows survive: two defeats and two protected"), Loaded->Participants.Num(), 4);
    for (const FName Id : DefeatedIds)
    {
        const auto* Row = Loaded->Participants.FindByPredicate([Id](const auto& P) { return P.ParticipantId == Id; });
        if (!TestNotNull(TEXT("Dead transferred membership survives without its actor"), Row)) { return false; }
        TestTrue(TEXT("Saved hostile role remains required with no actor"), Row->bRequiredForVictory && !Row->bAllowMassRepresentation && Row->Character == nullptr);
    }
    TestFalse(TEXT("A missing living actor is never relabeled as a confirmed defeat"),
        Loaded->Participants.ContainsByPredicate([EliteId](const auto& P) { return P.ParticipantId == EliteId; }));
    TestEqual(TEXT("Saved defeat count cannot grow"), FSovCrucibleRuntimeTestAccess::SavedDefeats(Loaded).Num(), DefeatCount);
    TestFalse(TEXT("A missing living elite cannot acquire thermal victory"), Loaded->HasConfirmedVictory());
    TestTrue(TEXT("Existing protected ownership reconstruction still succeeds"), FSovCrucibleRuntimeTestAccess::ProtectionValid(Loaded));
    TestTrue(TEXT("Relinquished source loads through the same native path"), Save->LoadActorFromRecord(PhaseA, SavedA));
    TestTrue(TEXT("The source cannot reacquire any transferred row"), PhaseA->Participants.IsEmpty());
    TestEqual(TEXT("Metadata restoration cannot publish any new campaign proof"), F.PC->GetCampaignState()->GetJournal().Num(), JournalCount);
    return true;
}

/**
 * Supplies the summon with a content-free add.
 *
 * Spawning the authored Enforcer here loads its appearance asynchronously, and that load completes
 * after the fixture world is destroyed: the NPC then initialises with no controller and the engine
 * asserts inside the activity component, landing on whichever unrelated test is running next. The
 * shipping summon still uses the authored definition, where a live world makes that ordinary.
 */
struct FSovAurelionEliteTestAccess
{
    static void SetSummonDefinition(USovGameplayAbility_AurelionEliteSummon& Ability, UNPCDefinition* Definition)
    { Ability.SummonDefinition = Definition; }
};

namespace
{
template <typename TAbility>
TAbility* GrantEliteAbility(UNarrativeAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle& OutHandle)
{
    OutHandle = ASC->GiveAbility(FGameplayAbilitySpec(TAbility::StaticClass(), 1));
    FGameplayAbilitySpec* const Spec = ASC->FindAbilitySpecFromHandle(OutHandle);
    return Spec ? Cast<TAbility>(Spec->GetPrimaryInstance()) : nullptr;
}

int32 CountLivingNPCs(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<ASovNPCCharacterBase> It(World); It; ++It) { if (IsValid(*It)) { ++Count; } }
    return Count;
}

/** The boss phase policy reads remaining health fraction, so set both ends of it explicitly. */
void SetEliteHealthFraction(UNarrativeAbilitySystemComponent* ASC, float Fraction)
{
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f * Fraction);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionEliteSummonTest,
    "ProjectVelkorran.Campaign.Aurelion.EliteSummonIsPhaseGatedAndNeverBecomesAVictoryParticipant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionEliteSummonTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true);
    if (!TestNotNull(TEXT("Ready crucible campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    auto* Elite = F.Director->GetParticipant(TEXT("Formation.Guard"));
    if (!TestNotNull(TEXT("Fixture supplies the elite participant"), Elite)) { return false; }
    auto* EliteASC = Elite->GetNarrativeAbilitySystemComponent();
    FGameplayAbilitySpecHandle Handle;
    auto* Summon = GrantEliteAbility<USovGameplayAbility_AurelionEliteSummon>(EliteASC, Handle);
    if (!TestNotNull(TEXT("Summon ability instance exists"), Summon)) { return false; }
    // Content-free add: this test is about phase gating and attempt scoping, not about loading art.
    auto* AddDefinition = NewObject<UNPCDefinition>(F.PC); F.PC->KeepAlive.Add(AddDefinition);
    AddDefinition->NPCClassPath = ASovCampaignMassRoundTripNPC::StaticClass();
    AddDefinition->bAllowMultipleInstances = true;
    FSovAurelionEliteTestAccess::SetSummonDefinition(*Summon, AddDefinition);

    const int32 ParticipantsBefore = F.Director->Participants.Num();
    const int32 NPCsBefore = CountLivingNPCs(F.World);

    // Adds stay out of the opening phase so the first stretch of the fight remains readable.
    SetEliteHealthFraction(EliteASC, 1.f);
    EliteASC->TryActivateAbility(Handle);
    TestEqual(TEXT("A boss at full health summons nothing"), Summon->GetLivingSummonCount(), 0);
    TestEqual(TEXT("A refused summon spawns no characters at all"), CountLivingNPCs(F.World), NPCsBefore);

    // The second phase opens at 66% remaining health.
    SetEliteHealthFraction(EliteASC, .5f);
    EliteASC->TryActivateAbility(Handle);
    const int32 Living = Summon->GetLivingSummonCount();
    if (!TestTrue(TEXT("A wounded boss brings in adds"), Living > 0)) { return false; }
    TestTrue(TEXT("Summons respect their living ceiling"), Living <= 4);
    TestEqual(TEXT("Every summon is a real spawned character"), CountLivingNPCs(F.World), NPCsBefore + Living);

    // The required roster is fixed before the fight starts. Summons pressure the player and are
    // cleaned up with the attempt; they never join the victory condition.
    TestEqual(TEXT("Summoning never adds a victory participant"), F.Director->Participants.Num(), ParticipantsBefore);
    int32 Unregistered = 0;
    for (TActorIterator<ASovNPCCharacterBase> It(F.World); It; ++It)
    {
        if (IsValid(*It) && F.Director->FindParticipantId(*It).IsNone()) { ++Unregistered; }
    }
    TestTrue(TEXT("The summoned adds are attempt-scoped rather than participants"), Unregistered >= Living);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionEliteLanceRangeTest,
    "ProjectVelkorran.Campaign.Aurelion.EliteLanceRefusesBeyondItsRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionEliteLanceRangeTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true);
    if (!TestNotNull(TEXT("Ready crucible campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    auto* Elite = F.Director->GetParticipant(TEXT("Formation.Guard"));
    if (!TestNotNull(TEXT("Fixture supplies the elite participant"), Elite)) { return false; }
    auto* EliteASC = Elite->GetNarrativeAbilitySystemComponent();
    FGameplayAbilitySpecHandle Handle;
    auto* Lance = GrantEliteAbility<USovGameplayAbility_AurelionEliteLance>(EliteASC, Handle);
    if (!TestNotNull(TEXT("Lance ability instance exists"), Lance)) { return false; }

    const float PlayerHealthBefore = F.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    // Well beyond the lance's authored reach.
    F.Player->SetActorLocation(Elite->GetActorLocation() + FVector(9000.f, 0.f, 0.f));
    EliteASC->TryActivateAbility(Handle);
    TestEqual(TEXT("A shot from outside its range never reaches the player"),
        F.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), PlayerHealthBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionEliteSlamTest,
    "ProjectVelkorran.Campaign.Aurelion.EliteSlamSparesItsOwnSide",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionEliteSlamTest::RunTest(const FString& Parameters)
{
    FEncounterObjectiveWorld F(true);
    if (!TestNotNull(TEXT("Ready crucible campaign"), F.ASC) || !F.Start()) { AddError(F.SetupError); return false; }
    auto* Elite = F.Director->GetParticipant(TEXT("Formation.Guard"));
    auto* Ally = F.Director->GetParticipant(TEXT("Crucible.NodeWest"));
    if (!TestNotNull(TEXT("Fixture supplies the elite participant"), Elite)
        || !TestNotNull(TEXT("Fixture supplies a same-side participant"), Ally)) { return false; }
    auto* EliteASC = Elite->GetNarrativeAbilitySystemComponent();
    auto* AllyASC = Ally->GetNarrativeAbilitySystemComponent();
    FGameplayAbilitySpecHandle Handle;
    auto* Slam = GrantEliteAbility<USovGameplayAbility_AurelionEliteSlam>(EliteASC, Handle);
    if (!TestNotNull(TEXT("Slam ability instance exists"), Slam)) { return false; }

    // Its own side is placed well inside the slam radius, where an indiscriminate payload would hit.
    Ally->SetActorLocation(Elite->GetActorLocation() + FVector(120.f, 0.f, 0.f));
    const float EliteHealthBefore = EliteASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    const float AllyHealthBefore = AllyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());

    EliteASC->TryActivateAbility(Handle);

    TestEqual(TEXT("The slam never damages the boss itself"),
        EliteASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), EliteHealthBefore);
    TestEqual(TEXT("The slam never damages its own side"),
        AllyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), AllyHealthBefore);
    return true;
}

#endif
