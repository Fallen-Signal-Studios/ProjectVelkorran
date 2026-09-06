// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovSaveSubsystem.h"
#include "Tests/SovMissionTravelRecoveryFixtures.h"
#include "Kismet/GameplayStatics.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Framework/SovCampaignGameMode.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Tests/SovSaveRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
struct FSovTravelTransactionTestAccess
{
    struct FUnusedStorage final : ISovSaveStorage
    {
        bool Read(const FString&, int32, TArray<uint8>&) override { return false; }
        bool Write(const FString&, int32, const TArray<uint8>&) override { return false; }
        bool Exists(const FString&, int32) override { return false; }
    };
    static FGuid Stage(USovSaveSubsystem& S, UWorld& World, USovCampaignDefinition& Mission)
    {
        Clear(S);
        S.Storage = MakeUnique<FUnusedStorage>();
        S.AccountNamespace = TEXT("travel-account-A"); S.UserIndex = 0;
        S.MissionTravelOwner = S.CaptureOperationOwner();
        S.MissionTravelOrigin = NewObject<USovCampaignSaveGame>(&S);
        S.MissionTravelOrigin->Header.AccountNamespace = S.AccountNamespace;
        S.MissionTravelOrigin->Header.Kind = ESovSaveSlotKind::Checkpoint;
        S.MissionTravelOrigin->Header.MissionId = Mission.MissionId;
        S.MissionTravelOrigin->Header.MissionDefinition = FSoftObjectPath(&Mission);
        S.MissionTravelOrigin->Header.MapPackage = World.GetOutermost()->GetName();
        S.MissionTravelOrigin->Header.Generation = 23;
        S.MissionTravelNarrative = NewObject<UNarrativeSave>(&S);
        S.MissionTravelRequest = FGuid::NewGuid();
        S.MissionTravelDestinationId = Mission.MissionId;
        S.MissionTravelDestinationDefinition = FSoftObjectPath(&Mission);
        S.MissionTravelDestinationMap = S.MissionTravelOrigin->Header.MapPackage;
        S.MissionTravelSourceWorld = &World;
        S.MissionTravelDeadline = FPlatformTime::Seconds() + 60.0;
        return S.MissionTravelRequest;
    }
    static void Arm(USovSaveSubsystem& S) { S.InitializeMissionTravelRecovery(); }
    static void Fail(USovSaveSubsystem& S, const FGuid& Request, UWorld* World)
    { S.RecordMissionTravelFailureForRequest(Request, World, TEXT("Injected accepted-map failure")); }
    static bool Failed(const USovSaveSubsystem& S) { return S.bMissionTravelFailurePending || S.bPendingLoadFailed; }
    static bool Matches(USovSaveSubsystem& S, const FString& URL)
    {
        UWorld* World = S.GetWorld();
        auto* Mode = World ? World->GetAuthGameMode<ASovCampaignGameMode>() : nullptr;
        if (!Mode) { return false; }
        TGuardValue<FString> Options(Mode->OptionsString, URL);
        FString Error;
        if (UGameplayStatics::HasOption(URL, TEXT("SovCampaignTransition")))
        { return S.ValidateMissionTravelWorld(*World, Error); }
        return S.MatchesPendingLoadRequest(URL);
    }
    static bool Recover(USovSaveSubsystem& S, FString& Error) { return S.BeginMissionOriginRecovery(Error); }
    static void CompleteFailure(USovSaveSubsystem& S) { S.CompletePendingLoad(false, TEXT("Recovery failed; retry origin checkpoint.")); }
    static void Clear(USovSaveSubsystem& S)
    {
        S.DeinitializeMissionTravelRecovery();
        S.PendingSave = nullptr; S.PendingNarrative = nullptr; S.PendingLoadRequest.Invalidate();
        S.PendingDestination.Reset(); S.bPendingLoadFailed = false; S.bPendingWorldApplied = false;
    }
    static bool OriginIntact(const USovSaveSubsystem& S)
    { return S.MissionTravelOrigin && S.MissionTravelOrigin->Header.Generation == 23; }
    static FGuid Request(const USovSaveSubsystem& S) { return S.PendingSave ? S.PendingLoadRequest : S.MissionTravelRequest; }
    static FGuid Operation(const USovSaveSubsystem& S) { return S.MissionTravelRequest; }
    static void SwitchAccount(USovSaveSubsystem& S) { S.AccountNamespace = TEXT("travel-account-B"); }
    static void BindGenerations(USovSaveSubsystem& S, ASovPlayerCharacterBase& Pawn, UNarrativeAbilitySystemComponent& ASC)
    {
        S.RestorePawn = &Pawn; S.RestoreASC = &ASC;
        S.RestoreASCEpoch = ASC.GetCombatActorInfoEpoch(); S.RestorePawnGeneration = Pawn.GetCharacterInitializationGeneration();
    }
    static bool GenerationsMatch(const USovSaveSubsystem& S) { return S.MatchesRestoreGenerations(); }
};
namespace
{
    struct FTravelWorld
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<USovMissionTravelTestSubsystem> Saves{NewObject<USovMissionTravelTestSubsystem>(Instance.Get())};
        TStrongObjectPtr<USovCampaignDefinition> Mission{NewObject<USovCampaignDefinition>()};
        UWorld* World = nullptr;
        FTravelWorld()
        {
            const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS, true);
            if (!World) { return; }
            World->SetGameInstance(Instance.Get()); Saves->TestWorld = World;
            if (GEngine) { auto& C = GEngine->CreateNewWorldContext(EWorldType::Game); C.SetCurrentWorld(World); C.OwningGameInstance = Instance.Get(); }
            World->InitWorld(IVS);
            FURL URL; URL.AddOption(TEXT("game=/Script/ProjectVelkorran.SovCampaignGameMode"));
            if (!World->SetGameMode(URL)) { return; }
            Mission->MissionId = TEXT("M_TravelTransaction"); Mission->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(World));
            World->GetAuthGameMode<ASovCampaignGameMode>()->InitialMission = Mission.Get();
        }
        ~FTravelWorld()
        {
            FSovTravelTransactionTestAccess::Clear(*Saves);
            if (World) { World->NextURL.Reset(); World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
        }
        bool Ready() const { return World && World->GetAuthGameMode<ASovCampaignGameMode>(); }
    };
    FString MissionURL(const FGuid& ID)
    { return TEXT("?SovCampaignTransition=1?SovMissionTravelRequest=") + ID.ToString(EGuidFormats::Digits); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelRequestPhaseIsolationTest,
    "ProjectVelkorran.Campaign.Travel.RequestPhaseAndWorldIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelRequestPhaseIsolationTest::RunTest(const FString& Parameters)
{
    FTravelWorld F;
    if (!TestTrue(TEXT("Campaign world initialized"), F.Ready())) { return false; }
    const FGuid Request = FSovTravelTransactionTestAccess::Stage(*F.Saves, *F.World, *F.Mission);
    auto* GM = F.World->GetAuthGameMode<ASovCampaignGameMode>(); GM->OptionsString = MissionURL(Request);
    FString Error;
    TestTrue(TEXT("Exact phase, mission asset, map and account admitted"), F.Saves->ValidateMissionTravelWorld(*F.World, Error));
    TestFalse(TEXT("A stale token cannot consume current records"), FSovTravelTransactionTestAccess::Matches(*F.Saves, MissionURL(FGuid::NewGuid())));
    TestFalse(TEXT("Same ID with wrong restore phase is refused"), FSovTravelTransactionTestAccess::Matches(*F.Saves,
        TEXT("?SovCampaignSlotLoad=1?SovCampaignLoadRequest=") + Request.ToString(EGuidFormats::Digits)));
    TestFalse(TEXT("Ambiguous URL cannot name both phases"), FSovTravelTransactionTestAccess::Matches(*F.Saves, MissionURL(Request) + TEXT("?SovCampaignSlotLoad=1")));
    GM->OptionsString = MissionURL(FGuid::NewGuid());
    TestFalse(TEXT("Late destination is closed before campaign spawn"), F.Saves->ValidateMissionTravelWorld(*F.World, Error));
    TestTrue(TEXT("Rejected stale destination preserves origin generation"), FSovTravelTransactionTestAccess::OriginIntact(*F.Saves));
    GM->OptionsString = MissionURL(Request); FSovTravelTransactionTestAccess::SwitchAccount(*F.Saves);
    TestFalse(TEXT("Valid token cannot cross platform accounts"), F.Saves->ValidateMissionTravelWorld(*F.World, Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelEngineFailureOwnershipTest,
    "ProjectVelkorran.Campaign.Travel.NativeFailureKeepsOriginAndRejectsOldCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelEngineFailureOwnershipTest::RunTest(const FString& Parameters)
{
    FTravelWorld F; FTravelWorld Other;
    if (!TestTrue(TEXT("Two independent campaign worlds initialized"), GEngine && F.Ready() && Other.Ready())) { return false; }
    const FGuid Request = FSovTravelTransactionTestAccess::Stage(*F.Saves, *F.World, *F.Mission);
    FSovTravelTransactionTestAccess::Arm(*F.Saves);
    GEngine->OnTravelFailure().Broadcast(Other.World, ETravelFailure::LoadMapFailure, TEXT("Other instance failed"));
    TestFalse(TEXT("Other game instance cannot fail the active operation"), FSovTravelTransactionTestAccess::Failed(*F.Saves));
    FSovTravelTransactionTestAccess::Fail(*F.Saves, FGuid::NewGuid(), F.World);
    TestFalse(TEXT("Captured old delegate token is ignored"), FSovTravelTransactionTestAccess::Failed(*F.Saves));
    GEngine->OnTravelFailure().Broadcast(F.World, ETravelFailure::LoadMapFailure, TEXT("Actual owned map failure"));
    TestTrue(TEXT("Native engine failure reaches the owning operation immediately"), FSovTravelTransactionTestAccess::Failed(*F.Saves));
    TestTrue(TEXT("No recovery travel occurs inside failure broadcast"), F.World->NextURL.IsEmpty());
    TestTrue(TEXT("Exact verified checkpoint remains alive until deferred recovery"), FSovTravelTransactionTestAccess::OriginIntact(*F.Saves));
    const FGuid NewRequest = FSovTravelTransactionTestAccess::Stage(*F.Saves, *F.World, *F.Mission);
    FSovTravelTransactionTestAccess::Fail(*F.Saves, Request, F.World);
    TestFalse(TEXT("Old completion cannot fail a newer operation in the same world"), FSovTravelTransactionTestAccess::Failed(*F.Saves));
    TestTrue(TEXT("New request identity remains intact"), FSovTravelTransactionTestAccess::Request(*F.Saves) == NewRequest);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelRecoverySingleAttemptTest,
    "ProjectVelkorran.Campaign.Travel.OriginRecoveryIsSingleAttemptAndRetainsExplicitRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelRecoverySingleAttemptTest::RunTest(const FString& Parameters)
{
    FTravelWorld F;
    if (!TestTrue(TEXT("Campaign world initialized"), F.Ready())) { return false; }
    const FGuid Request = FSovTravelTransactionTestAccess::Stage(*F.Saves, *F.World, *F.Mission);
    F.World->NextURL = TEXT("/Game/FailedDestination") + MissionURL(Request);
    FString Error = TEXT("Destination package failed after acceptance.");
    TestTrue(TEXT("Origin recovery asks Unreal for the saved origin map"), FSovTravelTransactionTestAccess::Recover(*F.Saves, Error));
    const FGuid RecoveryRequest = FSovTravelTransactionTestAccess::Request(*F.Saves);
    TestTrue(TEXT("Phase request rotates while operation identity remains"), RecoveryRequest != Request
        && FSovTravelTransactionTestAccess::Operation(*F.Saves) == Request);
    TestTrue(TEXT("The recovery URL includes its new phase ID"), F.Saves->RequestedOptions.Contains(RecoveryRequest.ToString(EGuidFormats::Digits)));
    TestTrue(TEXT("Recovery keeps exact original checkpoint generation"), FSovTravelTransactionTestAccess::OriginIntact(*F.Saves));
    TestFalse(TEXT("A failed recovery cannot schedule another automatic recovery"), FSovTravelTransactionTestAccess::Recover(*F.Saves, Error));
    TestFalse(TEXT("Late original destination token no longer matches"), FSovTravelTransactionTestAccess::Matches(*F.Saves, MissionURL(Request)));
    FSovTravelTransactionTestAccess::CompleteFailure(*F.Saves);
    TestFalse(TEXT("Failed recovery releases pending ownership"), F.Saves->IsLoadPending());
    TestTrue(TEXT("Explicit recovery remains available after terminal failure"), F.Saves->HasTravelRecovery());
    FSovTravelTransactionTestAccess::SwitchAccount(*F.Saves);
    TestFalse(TEXT("Retained origin cannot be retried by another account"), F.Saves->HasTravelRecovery());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTravelRestoreGenerationTest,
    "ProjectVelkorran.Campaign.Travel.SamePointerASCRebindRetiresRestoreLease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTravelRestoreGenerationTest::RunTest(const FString& Parameters)
{
    FTravelWorld F;
    if (!TestTrue(TEXT("Campaign world initialized"), F.Ready())) { return false; }
    auto* Pawn = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
    auto* Owner = F.World->SpawnActor<AActor>();
    if (!Pawn || !Owner) { AddError(TEXT("Restore generation fixture could not spawn")); return false; }
    TStrongObjectPtr<UNarrativeAbilitySystemComponent> ASC(NewObject<UNarrativeAbilitySystemComponent>(Owner));
    Owner->AddInstanceComponent(ASC.Get()); ASC->RegisterComponent();
    if (!TestTrue(TEXT("Restore ASC registered its actor-info storage"), ASC->IsRegistered() && ASC->AbilityActorInfo.IsValid())) { return false; }
    ASC->InitAbilityActorInfo(Owner, Pawn);
    FSovTravelTransactionTestAccess::BindGenerations(*F.Saves, *Pawn, *ASC);
    TestTrue(TEXT("Current actor-info generation is accepted"), FSovTravelTransactionTestAccess::GenerationsMatch(*F.Saves));
    ASC->ClearActorInfo(); ASC->InitAbilityActorInfo(Owner, Pawn);
    TestTrue(TEXT("Actor-info pointers returned to exactly the same pawn"), ASC->GetAvatarActor() == Pawn);
    TestFalse(TEXT("Retire/rebind cannot reuse the old restore generation"), FSovTravelTransactionTestAccess::GenerationsMatch(*F.Saves));
    FSovTravelTransactionTestAccess::BindGenerations(*F.Saves, *Pawn, *ASC);
    TestTrue(TEXT("An explicitly new lease accepts the new actor-info generation"), FSovTravelTransactionTestAccess::GenerationsMatch(*F.Saves));
    return true;
}
#endif
