// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMissionTravelRecoveryFixtures.h"
#include "Tests/SovSaveRuntimeTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Framework/SovCampaignGameMode.h"
#include "Misc/AutomationTest.h"
#include "NarrativeSave.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
struct FSovMissionTravelTestAccess
{
    struct FUnusedStorage final : ISovSaveStorage
    {
        bool Read(const FString&, int32, TArray<uint8>&) override { return false; }
        bool Write(const FString&, int32, const TArray<uint8>&) override { return false; }
        bool Exists(const FString&, int32) override { return false; }
    };
    static FGuid Stage(USovSaveSubsystem& S, UWorld* World)
    {
        S.Storage = MakeUnique<FUnusedStorage>();
        S.AccountNamespace = TEXT("origin-owner"); S.UserIndex = 0;
        S.MissionTravelOrigin = NewObject<USovCampaignSaveGame>(&S);
        S.MissionTravelOrigin->Header.MapPackage = TEXT("/Game/Tests/VerifiedOrigin");
        S.MissionTravelOrigin->Header.Generation = 17;
        S.MissionTravelNarrative = NewObject<UNarrativeSave>(&S);
        S.MissionTravelOwner = S.CaptureOperationOwner();
        S.MissionTravelRequest = FGuid::NewGuid(); S.MissionTravelSourceWorld = World;
        S.MissionTravelDeadline = FPlatformTime::Seconds() + 120.0;
        return S.MissionTravelRequest;
    }
    static void Bind(USovSaveSubsystem& S) { S.InitializeMissionTravelRecovery(); }
    static void Unbind(USovSaveSubsystem& S) { S.DeinitializeMissionTravelRecovery(); }
    static void Tick(USovSaveSubsystem& S) { S.TickMissionTravelRecovery(); }
    static void Fail(USovSaveSubsystem& S, UWorld* World) { S.RecordMissionTravelFailure(World, TEXT("Injected accepted-travel failure")); }
    static void Expire(USovSaveSubsystem& S) { S.MissionTravelDeadline = FPlatformTime::Seconds() - 1.; }
    static bool ExactOrigin(const USovSaveSubsystem& S)
    { return S.PendingSave == S.MissionTravelOrigin && S.PendingNarrative && S.PendingNarrative != S.MissionTravelNarrative
        && S.PendingNarrative->RecordMap.Num() == S.MissionTravelNarrative->RecordMap.Num() && S.PendingSave->Header.Generation == 17; }
    static void MutateAttempt(USovSaveSubsystem& S) { S.PendingNarrative->RecordMap.Add(FGuid::NewGuid(), FNarrativeActorRecord()); }
    static void Complete(USovSaveSubsystem& S, bool bSucceeded) { S.CompletePendingLoad(bSucceeded, TEXT("Injected completion")); }
    static void Reauthorize(USovSaveSubsystem& S) { ++S.AuthorizationEpoch; }
    static void SwitchAccount(USovSaveSubsystem& S) { ++S.SelectionEpoch; S.AccountNamespace = TEXT("different-owner"); }
    static bool Retained(const USovSaveSubsystem& S) { return S.MissionTravelOrigin != nullptr; }
    static bool PendingFailed(const USovSaveSubsystem& S) { return S.bPendingLoadFailed; }
    static void ExpectDestination(USovSaveSubsystem& S, USovCampaignDefinition* Mission)
    {
        S.MissionTravelDestinationId = Mission->MissionId;
        S.MissionTravelDestinationDefinition = FSoftObjectPath(Mission);
        S.MissionTravelDestinationMap = Mission->Map.ToSoftObjectPath().GetLongPackageName();
    }
};

namespace SovMissionTravelTests
{
    struct FFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{ NewObject<UGameInstance>() };
        TStrongObjectPtr<USovMissionTravelTestSubsystem> Slots{ NewObject<USovMissionTravelTestSubsystem>(Instance.Get()) };
        UWorld* World = nullptr;
        FFixture()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            if (World)
            {
                World->SetGameInstance(Instance.Get()); Slots->TestWorld = World;
                if (GEngine)
                {
                    auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
                    Context.SetCurrentWorld(World); Context.OwningGameInstance = Instance.Get();
                }
            }
        }
        ~FFixture()
        {
            FSovMissionTravelTestAccess::Unbind(*Slots);
            if (World)
            {
                World->DestroyWorld(false);
                if (GEngine) { GEngine->DestroyWorldContext(World); }
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelAcceptedFailureTest,
    "ProjectVelkorran.Campaign.Save.MissionTravel.AcceptedFailureUsesExactOriginOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelAcceptedFailureTest::RunTest(const FString&)
{
    SovMissionTravelTests::FFixture F; if (!TestNotNull(TEXT("World"), F.World)) { return false; }
    FSovMissionTravelTestAccess::Stage(*F.Slots, F.World); FSovMissionTravelTestAccess::Bind(*F.Slots);
    if (GEngine) { GEngine->OnTravelFailure().Broadcast(F.World, ETravelFailure::LoadMapFailure, TEXT("Injected load failure")); }
    else { FSovMissionTravelTestAccess::Fail(*F.Slots, F.World); }
    TestEqual(TEXT("No reentrant engine travel in failure delegate"), F.Slots->RecoveryRequests, 0);
    FSovMissionTravelTestAccess::Tick(*F.Slots);
    TestEqual(TEXT("Core continuation requests one origin load without a source controller"), F.Slots->RecoveryRequests, 1);
    TestTrue(TEXT("Existing pending load owns retained envelope and an isolated Narrative attempt"), FSovMissionTravelTestAccess::ExactOrigin(*F.Slots));
    TestTrue(TEXT("Recovery URL uses the existing GUID-correlated slot loader"), F.Slots->RequestedOptions.Contains(TEXT("SovCampaignLoadRequest=")));
    FSovMissionTravelTestAccess::Fail(*F.Slots, F.World); FSovMissionTravelTestAccess::Tick(*F.Slots);
    TestFalse(TEXT("Late source teardown cannot fail the newer recovery request"), FSovMissionTravelTestAccess::PendingFailed(*F.Slots));
    TestEqual(TEXT("Recovery failure never loops automatically"), F.Slots->RecoveryRequests, 1);
    FSovMissionTravelTestAccess::MutateAttempt(*F.Slots);
    FSovMissionTravelTestAccess::Complete(*F.Slots, false);
    TestFalse(TEXT("Failed recovery releases active ownership"), F.Slots->IsMissionTravelPending() || F.Slots->IsLoadPending());
    TestTrue(TEXT("Failed recovery retains exact origin for explicit retry"), FSovMissionTravelTestAccess::Retained(*F.Slots));
    FString Error; TestTrue(TEXT("Original owner may explicitly retry"), F.Slots->RetryMissionTravelRecovery(Error));
    TestTrue(TEXT("Retry starts from immutable origin, not failed attempt mutations"), FSovMissionTravelTestAccess::ExactOrigin(*F.Slots));
    TestEqual(TEXT("Explicit retry issues exactly one new request"), F.Slots->RecoveryRequests, 2);
    FSovMissionTravelTestAccess::Complete(*F.Slots, true);
    TestFalse(TEXT("Successful origin readiness retires recovery transaction"), FSovMissionTravelTestAccess::Retained(*F.Slots));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelOwnerEpochTest,
    "ProjectVelkorran.Campaign.Save.MissionTravel.AuthorizationABACannotAutoRecover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelOwnerEpochTest::RunTest(const FString&)
{
    SovMissionTravelTests::FFixture F; if (!F.World) { return false; }
    FSovMissionTravelTestAccess::Stage(*F.Slots, F.World); FSovMissionTravelTestAccess::Reauthorize(*F.Slots);
    FSovMissionTravelTestAccess::Tick(*F.Slots);
    TestEqual(TEXT("Reauthorization is not permission for old automatic work"), F.Slots->RecoveryRequests, 0);
    TestFalse(TEXT("Old active request is released"), F.Slots->IsMissionTravelPending());
    FString Error; TestTrue(TEXT("Explicit same-owner retry captures a fresh token"), F.Slots->RetryMissionTravelRecovery(Error));
    FSovMissionTravelTestAccess::Complete(*F.Slots, false); FSovMissionTravelTestAccess::SwitchAccount(*F.Slots);
    TestFalse(TEXT("A different selected account cannot load retained origin"), F.Slots->RetryMissionTravelRecovery(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelStaleFailureTest,
    "ProjectVelkorran.Campaign.Save.MissionTravel.StaleWorldAndCancelledRequestIgnored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelStaleFailureTest::RunTest(const FString&)
{
    SovMissionTravelTests::FFixture F, Other; if (!F.World || !Other.World) { return false; }
    const FGuid Request = FSovMissionTravelTestAccess::Stage(*F.Slots, F.World);
    FSovMissionTravelTestAccess::Fail(*F.Slots, Other.World); FSovMissionTravelTestAccess::Tick(*F.Slots);
    TestEqual(TEXT("Other GameInstance failure cannot trigger this transaction"), F.Slots->RecoveryRequests, 0);
    F.Slots->CancelMissionTravelRecovery(FGuid::NewGuid());
    TestTrue(TEXT("Stale cancellation cannot retire current travel"), F.Slots->IsMissionTravelPending());
    F.Slots->CancelMissionTravelRecovery(Request); FSovMissionTravelTestAccess::Fail(*F.Slots, F.World);
    FSovMissionTravelTestAccess::Tick(*F.Slots);
    TestEqual(TEXT("Synchronous rejection leaves no late failure continuation"), F.Slots->RecoveryRequests, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelSuspendDeadlineTest,
    "ProjectVelkorran.Campaign.Save.MissionTravel.SuspendHoldsTimeout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelSuspendDeadlineTest::RunTest(const FString&)
{
    SovMissionTravelTests::FFixture F; if (!F.World) { return false; }
    FSovMissionTravelTestAccess::Stage(*F.Slots, F.World); F.Slots->SetPlatformSuspended(true);
    FSovMissionTravelTestAccess::Expire(*F.Slots); FSovMissionTravelTestAccess::Tick(*F.Slots);
    TestEqual(TEXT("Suspended application never initiates recovery I/O/travel"), F.Slots->RecoveryRequests, 0);
    F.Slots->SetPlatformSuspended(false); FSovMissionTravelTestAccess::Expire(*F.Slots);
    FSovMissionTravelTestAccess::Tick(*F.Slots);
    TestEqual(TEXT("Same-owner foreground timeout may recover"), F.Slots->RecoveryRequests, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelAdmissionTest,
    "ProjectVelkorran.Campaign.Save.MissionTravel.RejectsCompetingLoadAndInvalidDestination",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelAdmissionTest::RunTest(const FString&)
{
    SovMissionTravelTests::FFixture F; if (!F.World) { return false; }
    const FGuid Request = FSovMissionTravelTestAccess::Stage(*F.Slots, F.World);
    FString Error;
    TestEqual(TEXT("Explicit slot load cannot supersede accepted mission travel"),
        F.Slots->LoadSlot(ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::Busy);
    FURL URL; URL.AddOption(TEXT("game=/Script/ProjectVelkorran.SovCampaignGameMode"));
    if (!TestTrue(TEXT("Campaign GameMode initializes"), F.World->SetGameMode(URL))) { return false; }
    auto* Mode = F.World->GetAuthGameMode<ASovCampaignGameMode>();
    if (!TestNotNull(TEXT("Campaign GameMode"), Mode)) { return false; }
    Mode->OptionsString = TEXT("?SovCampaignTransition=1?SovMissionTravelRequest=") + Request.ToString(EGuidFormats::Digits);
    Mode->InitialMission = nullptr;
    TestFalse(TEXT("A destination without an authored mission fails closed"), F.Slots->ValidateMissionTravelWorld(*F.World, Error));
    TStrongObjectPtr<USovCampaignDefinition> Mission(NewObject<USovCampaignDefinition>());
    Mission->MissionId = TEXT("M02_TravelRegression");
    Mission->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/DifferentMap.DifferentMap")));
    Mode->InitialMission = Mission.Get(); FSovMissionTravelTestAccess::ExpectDestination(*F.Slots, Mission.Get());
    TestFalse(TEXT("Matching definition on a different actual map is insufficient"), F.Slots->ValidateMissionTravelWorld(*F.World, Error));
    Mode->OptionsString = TEXT("?SovCampaignTransition=1?SovMissionTravelRequest=") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TestFalse(TEXT("Stale destination request is never admitted"), F.Slots->ValidateMissionTravelWorld(*F.World, Error));
    return true;
}
#endif
