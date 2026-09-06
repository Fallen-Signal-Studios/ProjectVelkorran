// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovNarrativeSerializerTestFixtures.h"
#include "Tests/SovSaveRuntimeTestFixtures.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProperties.h"
#include "Framework/SovCampaignGameMode.h"
#include "Misc/AutomationTest.h"
#include "Save/SovSaveSubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
struct FSovSaveWorldLoadTestAccess
{
    static FDelegateHandle Stage(USovSaveSubsystem& S, USovCampaignDefinition* Mission,
        UNarrativeSave* Snapshot, const FGuid& Request)
    {
        S.PendingSave = NewObject<USovCampaignSaveGame>(&S);
        S.AccountNamespace = TEXT("world-load-test-account");
        S.PendingAccount = S.AccountNamespace; S.PendingUser = S.UserIndex;
        S.PendingSave->Header.AccountNamespace = S.AccountNamespace;
        S.PendingSave->Header.MapPackage = Mission->Map.ToSoftObjectPath().GetLongPackageName();
        S.PendingSave->Header.MissionId = Mission->MissionId;
        S.PendingSave->Header.MissionDefinition = FSoftObjectPath(Mission);
        S.PendingNarrative = Snapshot;
        S.PendingLoadRequest = Request;
        S.PendingLoadDeadline = FPlatformTime::Seconds() + 60.0;
        S.bPendingWorldApplied = false; S.bPendingLoadFailed = false;
        return UNarrativeSaveSubsystem::OnInitialSaveRequested.AddUObject(&S, &USovSaveSubsystem::ResolveInitialSave);
    }
    static bool AppliedTo(const USovSaveSubsystem& S, const UWorld* World)
    { return S.bPendingWorldApplied && S.PendingDestination.Get() == World; }
    static void Tick(USovSaveSubsystem& S) { S.Tick(0.1f); }
};

namespace
{
    struct FSaveLoadWorlds
    {
        TStrongObjectPtr<UGameInstance> Instance{ NewObject<UGameInstance>() };
        TArray<UWorld*> Worlds;
        FDelegateHandle InitialSaveHandle;
        UWorld* Create()
        {
            const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &WorldInitialization, true);
            if (!World) { return nullptr; }
            Worlds.Add(World);
            World->SetGameInstance(Instance.Get());
            if (GEngine)
            {
                auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
                Context.SetCurrentWorld(World); Context.OwningGameInstance = Instance.Get();
            }
            World->InitWorld(WorldInitialization);
            World->UpdateWorldComponents(!FPlatformProperties::RequiresCookedData(), false);
            FURL URL;
            URL.AddOption(TEXT("game=/Script/ProjectVelkorran.SovCampaignGameMode"));
            return World->SetGameMode(URL) ? World : nullptr;
        }
        ~FSaveLoadWorlds()
        {
            UNarrativeSaveSubsystem::OnInitialSaveRequested.Remove(InitialSaveHandle);
            for (UWorld* World : Worlds)
            {
                World->DestroyWorld(false);
                if (GEngine) { GEngine->DestroyWorldContext(World); }
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveAcceptedWorldRejectionTest,
    "ProjectVelkorran.Campaign.Save.AcceptedWorldSerializerFailureAndFreshCampaign",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveAcceptedWorldRejectionTest::RunTest(const FString& Parameters)
{
    FSaveLoadWorlds Fixture;
    UWorld* FailedWorld = Fixture.Create();
    UWorld* FreshWorld = Fixture.Create();
    if (!FailedWorld || !FreshWorld) { AddError(TEXT("Campaign test worlds could not initialize")); return false; }
    auto* Mode = FailedWorld->GetAuthGameMode<ASovCampaignGameMode>();
    auto* FreshMode = FreshWorld->GetAuthGameMode<ASovCampaignGameMode>();
    auto* Narrative = FailedWorld->GetSubsystem<UNarrativeSaveSubsystem>();
    auto* Actor = FailedWorld->SpawnActor<ASovSerializerMovableActor>();
    if (!Mode || !FreshMode || !Narrative || !Actor) { AddError(TEXT("Required campaign world fixtures are absent")); return false; }

    TStrongObjectPtr<USovCampaignDefinition> Mission(NewObject<USovCampaignDefinition>());
    Mission->MissionId = TEXT("M01_SerializerRegression");
    Mission->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(FailedWorld));
    Mode->InitialMission = Mission.Get(); FreshMode->InitialMission = Mission.Get();
    const FGuid Request = FGuid::NewGuid();
    Mode->OptionsString = TEXT("?SovCampaignSlotLoad=1?SovCampaignLoadRequest=") + Request.ToString(EGuidFormats::Digits);
    FreshMode->OptionsString.Reset();

    TStrongObjectPtr<UNarrativeSave> Snapshot(NewObject<UNarrativeSave>());
    FNarrativeActorRecord Record;
    if (!TestTrue(TEXT("Actor produces a valid save record"), Narrative->CreateActorRecord(Actor, Record))) { return false; }
    Snapshot->RecordMap.Add(Actor->Guid, Record);
    Actor->bRejectLoading = true; // Capture and preflight succeed; only actual actor deserialization fails.
    TStrongObjectPtr<USovSaveSubsystem> Slots(NewObject<USovSaveSubsystem>(Fixture.Instance.Get()));
    TStrongObjectPtr<USovSaveLoadCompletionProbe> Probe(NewObject<USovSaveLoadCompletionProbe>());
    Probe->Subsystem = Slots.Get(); Slots->OnLoadCompleted.AddDynamic(Probe.Get(), &USovSaveLoadCompletionProbe::OnCompleted);
    Fixture.InitialSaveHandle = FSovSaveWorldLoadTestAccess::Stage(*Slots, Mission.Get(), Snapshot.Get(), Request);

    Narrative->InitializeSaveSystem(*FailedWorld);
    TestTrue(TEXT("The exact requested world receives its decoded snapshot"), FSovSaveWorldLoadTestAccess::AppliedTo(*Slots, FailedWorld));
    TestTrue(TEXT("Actual actor archive failure rejects Narrative initial restore"), Narrative->DidInitialLoadFail());
    TestEqual(TEXT("Failure is deferred until initialization unwinds"), Probe->Notifications, 0);
    FSovSaveWorldLoadTestAccess::Tick(*Slots);
    TestEqual(TEXT("Accepted-world failure terminates immediately rather than waiting for timeout"), Probe->Notifications, 1);
    TestEqual(TEXT("Restore failure offers recovery"), Probe->LastResult, ESovSaveResult::RecoveryAvailable);
    TestTrue(TEXT("Recovery callback observes released request ownership"), Probe->bObservedReleasedOwnership);
    FString Error;
    TestFalse(TEXT("Rejected slot world cannot initialize a replacement campaign"), Slots->ValidatePendingWorld(*FailedWorld, Error));
    TestTrue(TEXT("Deliberate fresh no-slot campaign is not poisoned by previous failure"), Slots->ValidatePendingWorld(*FreshWorld, Error));
    TestTrue(TEXT("Actual failure reason is not reported as a timeout"), !Probe->LastMessage.IsEmpty() && !Probe->LastMessage.Contains(TEXT("timed out")));

    Actor->bRejectLoading = false;
    Narrative->InitializeSaveSystem(*FailedWorld); // Late repeat of the old travel URL cannot restore or start a new game.
    TestTrue(TEXT("Expired request remains rejected even after the actor is repaired"), Narrative->DidInitialLoadFail());
    FSovSaveWorldLoadTestAccess::Tick(*Slots);
    TestEqual(TEXT("Late callback cannot publish a second completion"), Probe->Notifications, 1);
    return true;
}
#endif
