// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovEncounterInitialEntryRuntimeFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "AI/NPCDefinition.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace
{
    struct FInitialEntryWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovHandoffRuntimeTestController* PC = nullptr;
        ASovInitialEntryOverlapPawn* Player = nullptr;
        ASovInitialEntryDelayedNPC* NPC = nullptr;
        ASovEncounterDirector* Director = nullptr;
        ASovCampaignEncounterObjective* Objective = nullptr;
        USovCampaignDefinition* Mission = nullptr;
        USovInitialEntryObserver* Observer = nullptr;
        bool bReady = false;
        uint64 Frame = GFrameCounter;
        explicit FInitialEntryWorld(bool bOptIn = true, bool bAllowMissionExit = false)
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World || !GEngine) { return; }
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL()); World->GetTimerManager().Tick(0.f);
            PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
            Player = World->SpawnActor<ASovInitialEntryOverlapPawn>();
            auto* PS = World->SpawnActor<ASovPlayerState>();
            if (!PC || !Player || !PS) { return; }
            World->AddController(PC);
            auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            Player->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Player);
            if (!Player->StageTestReadiness(PS, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
            auto* Ground = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Ground);
            Ground->SetRootComponent(Box); Ground->AddInstanceComponent(Box);
            Box->SetBoxExtent(FVector(2000, 2000, 10)); Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->RegisterComponent(); Ground->SetActorLocation(FVector(0, 0, -10));
            Player->SetActorLocation(FVector(-600, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f));
            Player->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
            Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Player->GetCapsuleComponent()->SetGenerateOverlapEvents(true);
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
            Mission->MissionId = TEXT("M12_InitialEntryRetryTest"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
            Mission->PawnClass = Player->GetClass(); Mission->PlayerDefinition = Definition;
            FSovCampaignBeatDefinition Beat;
            Beat.BeatId = TEXT("InitialEncounter"); Beat.RequiredProtagonist = Mission->Protagonist;
            Beat.RequiredEncounterId = TEXT("Test.InitialEntry"); Beat.ObjectiveText = FText::FromString(TEXT("Clear the entry"));
            Beat.bOptional = bAllowMissionExit;
            Mission->Beats.Add(Beat);
            if (bAllowMissionExit)
            {
                FSovCampaignBeatDefinition Exit; Exit.BeatId = TEXT("FixtureMissionExit");
                Exit.RequiredProtagonist = Mission->Protagonist; Exit.ObjectiveText = FText::FromString(TEXT("Leave this test mission"));
                Mission->Beats.Add(Exit); Mission->AllowedSuccessorMissions.Add(TEXT("M13_OtherInitialEntryMission"));
            }
            if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { return; }
            Director = World->SpawnActor<ASovEncounterDirector>(); Director->EncounterId = Beat.RequiredEncounterId;
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            NPC = World->SpawnActor<ASovInitialEntryDelayedNPC>(ASovInitialEntryDelayedNPC::StaticClass(), FVector(600, 0, 90), FRotator::ZeroRotator, Spawn);
            if (!NPC) { return; }
            auto* NPCDefinition = NewObject<UNPCDefinition>(PC); PC->KeepAlive.Add(NPCDefinition);
            NPC->SetNPCDefinition(NPCDefinition);
            auto* NPCASC = NPC->GetNarrativeAbilitySystemComponent();
#define SOV_ENTRY_RESOURCE(Name) NPCASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMax##Name##Attribute(), 100.f); NPCASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), 100.f);
            SOV_ENTRY_RESOURCE(Health) SOV_ENTRY_RESOURCE(Shield) SOV_ENTRY_RESOURCE(Stamina) SOV_ENTRY_RESOURCE(Poise) SOV_ENTRY_RESOURCE(Echo)
#undef SOV_ENTRY_RESOURCE
            if (!Director->RegisterParticipant(TEXT("Guard"), NPC, true)) { return; }
            Objective = World->SpawnActor<ASovCampaignEncounterObjective>();
            Objective->EncounterDirector = Director; Objective->MissionId = Mission->MissionId; Objective->CompletionBeat = Beat.BeatId;
            Objective->bStartOnPlayerOverlap = true; Objective->bRetryInitialEntryWhileOverlapping = bOptIn;
            Objective->SetActorLocation(FVector(0, 0, 100));
            Observer = NewObject<USovInitialEntryObserver>(PC); PC->KeepAlive.Add(Observer);
            Director->OnEncounterStateChanged.AddDynamic(Observer, &USovInitialEntryObserver::Changed);
            auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
            if (!Save || !Save->UpdateSaveObject(true)) { return; }
            // Other fixture actors supply deterministic initialized assets. Enable the
            // engine world phase required for real component overlap notifications,
            // then begin both the moving character's actor/components and the
            // production objective. Primitive overlap updates require both gates.
            World->SetBegunPlay(true); Player->DispatchBeginPlay(); Objective->DispatchBeginPlay();
            NPC->SetSnapshotReadyForTest(false);
            bReady = Player->IsCharacterReady() && Player->IsAlive() && NPC->IsAlive()
                && Player->HasActorBegunPlay() && Player->GetCapsuleComponent()->IsQueryCollisionEnabled()
                && Player->GetCapsuleComponent()->GetGenerateOverlapEvents()
                && !Objective->StartVolume->IsOverlappingComponent(Player->GetCapsuleComponent());
        }
        ~FInitialEntryWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        bool Enter()
        {
            return Player->SetActorLocation(FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f))
                && Objective->StartVolume->IsOverlappingComponent(Player->GetCapsuleComponent());
        }
        bool Leave()
        {
            return Player->SetActorLocation(FVector(-600, 0, Player->GetActorLocation().Z))
                && !Objective->StartVolume->IsOverlappingComponent(Player->GetCapsuleComponent());
        }
        void Advance(float Seconds = .21f)
        { TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame); World->GetTimerManager().Tick(Seconds); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInitialEntryReadyRetryTest,
    "ProjectVelkorran.Campaign.EncounterObjective.InitialEntryRetriesDelayedNPCInsideExactlyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInitialEntryReadyRetryTest::RunTest(const FString& Parameters)
{
    FInitialEntryWorld F;
    if (!TestTrue(TEXT("Real initialized player and delayed native NPC fixture"), F.bReady)
        || !TestTrue(TEXT("Moving player has entered its actual actor/component BeginPlay lifecycle"), F.Player->HasActorBegunPlay())
        || !TestTrue(TEXT("Moving player's actual capsule participates in collision queries"), F.Player->GetCapsuleComponent()->IsQueryCollisionEnabled())
        || !TestTrue(TEXT("Actual capsule movement creates the entry overlap"), F.Enter())) { return false; }
    TestEqual(TEXT("Unavailable NPC prevents the first ordinary entry"), F.Director->GetEncounterState(), ESovEncounterState::Inactive);
    TestFalse(TEXT("Failed startup cannot capture a partial NPC checkpoint"), F.Director->HasEncounterPlayer(F.Player));
    TestFalse(TEXT("Actual rejection reason is retained"), F.Objective->LastError.IsEmpty());
    F.Advance();
    TestEqual(TEXT("Still unavailable after one polling interval"), F.Director->GetEncounterState(), ESovEncounterState::Inactive);
    F.NPC->SetSnapshotReadyForTest(true); F.Advance();
    TestEqual(TEXT("Readiness while still physically inside starts the native encounter"), F.Director->GetEncounterState(), ESovEncounterState::Active);
    TestTrue(TEXT("The ordinary director captured this real player"), F.Director->HasEncounterPlayer(F.Player));
    TestTrue(TEXT("The ordinary director created a real attempt"), F.Director->GetAttemptId().IsValid());
    const FGuid Attempt = F.Director->GetAttemptId();
    for (int32 Index = 0; Index < 10; ++Index) { F.Advance(); }
    TestEqual(TEXT("Initial polling publishes Active exactly once"), F.Observer->Activations, 1);
    TestEqual(TEXT("Later timer intervals preserve the same live attempt"), F.Director->GetAttemptId(), Attempt);
    TestEqual(TEXT("Starting combat does not manufacture its victory"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    TestTrue(TEXT("A genuine failure can occur after this initial start"), F.Director->FailEncounter());
    for (int32 Index = 0; Index < 5; ++Index) { F.Advance(); }
    TestEqual(TEXT("Remaining inside never auto-retries failed combat"), F.Director->GetEncounterState(), ESovEncounterState::Failed);
    TestEqual(TEXT("Failed combat keeps its original attempt"), F.Director->GetAttemptId(), Attempt);
    TestEqual(TEXT("Failed combat cannot trigger a second activation"), F.Observer->Activations, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInitialEntryRetirementTest,
    "ProjectVelkorran.Campaign.EncounterObjective.InitialEntryPollingRetiresOnExitMissionAndPossession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInitialEntryRetirementTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 5; ++Case)
    {
        FInitialEntryWorld F(true, Case == 1);
        if (!TestTrue(TEXT("Ready initial-entry fixture"), F.bReady) || !TestTrue(TEXT("Real initial overlap"), F.Enter())) { return false; }
        if (Case == 0) { TestTrue(TEXT("Physical departure produces an end overlap"), F.Leave()); }
        else if (Case == 1)
        {
            auto* Replacement = DuplicateObject<USovCampaignDefinition>(F.Mission, F.PC); F.PC->KeepAlive.Add(Replacement);
            Replacement->MissionId = TEXT("M13_OtherInitialEntryMission");
            Replacement->AllowedSuccessorMissions.Reset();
            TestEqual(TEXT("A legal ordinary fixture exit completes its mission without a combat fact"),
                F.PC->GetCampaignState()->CompleteBeat(TEXT("FixtureMissionExit")), ESovCampaignResult::Applied);
            TestEqual(TEXT("A genuine new mission retires old entry polling"), F.PC->GetCampaignState()->BeginMission(Replacement), ESovCampaignResult::Applied);
        }
        else if (Case == 2) { F.PC->UnPossess(); }
        else if (Case == 3) { F.Objective->Destroy(); }
        else { F.Player->GetNarrativeAbilitySystemComponent()->SetCharacterReadyEpoch(F.Player->GetNarrativeAbilitySystemComponent()->GetCharacterReadyEpoch()+1); }
        F.NPC->SetSnapshotReadyForTest(true);
        for (int32 Index = 0; Index < 5; ++Index) { F.Advance(); }
        TestEqual(TEXT("Retired overlap never starts the old encounter"), F.Director->GetEncounterState(), ESovEncounterState::Inactive);
        TestFalse(TEXT("Retired overlap never captures a player checkpoint"), F.Director->HasEncounterPlayer(F.Player));
        TestEqual(TEXT("No retired activation event"), F.Observer->Activations, 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInitialEntryOptInTest,
    "ProjectVelkorran.Campaign.EncounterObjective.InitialEntryPollingIsOptInAndRequiresPhysicalOverlap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInitialEntryOptInTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Existing authored objectives retain their original behavior"), GetDefault<ASovCampaignEncounterObjective>()->bRetryInitialEntryWhileOverlapping);
    FInitialEntryWorld F(false);
    if (!TestTrue(TEXT("Ready non-opted fixture"), F.bReady) || !TestTrue(TEXT("Real entry overlap"), F.Enter())) { return false; }
    F.NPC->SetSnapshotReadyForTest(true);
    for (int32 Index = 0; Index < 5; ++Index) { F.Advance(); }
    TestEqual(TEXT("Without opt-in, readiness does not create an unsolicited retry"), F.Director->GetEncounterState(), ESovEncounterState::Inactive);
    TestTrue(TEXT("Leave the original volume"), F.Leave());
    F.Objective->bRetryInitialEntryWhileOverlapping = true;
    for (int32 Index = 0; Index < 5; ++Index) { F.Advance(); }
    TestEqual(TEXT("Opt-in alone cannot start a player outside the volume"), F.Director->GetEncounterState(), ESovEncounterState::Inactive);
    if (!TestTrue(TEXT("A fresh physical re-entry retains ordinary startup"), F.Enter())) { return false; }
    TestEqual(TEXT("The ready ordinary entry starts immediately"), F.Director->GetEncounterState(), ESovEncounterState::Active);
    TestEqual(TEXT("Exactly one normal activation"), F.Observer->Activations, 1);
    return true;
}
#endif
