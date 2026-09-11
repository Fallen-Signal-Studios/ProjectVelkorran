// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovFatalPresentationRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Character/PlayerDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Recovery/SovFatalRecoveryComponent.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
struct FSovFatalPresentationTestAccess
{
    static bool HasPending(const ASovPlayerController* PC) { return PC->FatalPresentationPawn.IsValid(); }
    static uint64 Serial(const ASovPlayerController* PC) { return PC->FatalPresentationSerial; }
    static void Poll(ASovPlayerController* PC, uint64 Serial) { PC->PollFatalPresentation(Serial); }
};
namespace
{
    struct FFatalPresentationWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovFatalPresentationTestController* PC = nullptr;
        ASovHandoffRuntimeTestPawn* Pawn = nullptr;
        ASovPlayerState* PS = nullptr;
        UNarrativeAbilitySystemComponent* ASC = nullptr;
        USovFatalPresentationTestHUD* HUD = nullptr;
        FFatalPresentationWorld()
        {
            const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            PC = World->SpawnActor<ASovFatalPresentationTestController>();
            Pawn = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
            PS = World->SpawnActor<ASovPlayerState>();
            if (!PC || !Pawn || !PS) { return; }
            auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            Pawn->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Pawn);
            if (!Pawn->StageTestReadiness(PS, true) || !Pawn->CompleteCampaignDataInitialization(false)) { return; }
            ASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
            HUD = NewObject<USovFatalPresentationTestHUD>(PC); PC->KeepAlive.Add(HUD);
            PC->SetTestHUD(HUD); PC->ConfigureFailureMenu(true);
        }
        ~FFatalPresentationWorld()
        {
            if (HUD) { HUD->OnOpen = nullptr; }
            if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
        }
        void Kill()
        {
            ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
            FGameplayEffectSpec Fatal(GetDefault<USovCombatRoutingTestEffect>(), ASC->MakeEffectContext(), 1.f);
            ASC->HandleOutOfHealth(nullptr, nullptr, Fatal, 100.f);
        }
        bool Restore()
        {
            FSovCombatResourceSnapshot Snapshot;
            if (!USovEncounterSnapshotLibrary::CaptureResources(ASC, Snapshot)) { return false; }
            Snapshot.Health = 100.f;
            return USovEncounterSnapshotLibrary::RebaseAuthoredResourceCurrents(ASC, Snapshot)
                && USovEncounterSnapshotLibrary::RestoreResources(ASC, Snapshot);
        }
        void Advance(float Seconds)
        {
            for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += .05f)
            { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFatalPresentationRecoveryTest,
    "ProjectVelkorran.Campaign.Recovery.RevivedPawnNeverStartsLegacyDeathDelay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFatalPresentationRecoveryTest::RunTest(const FString& Parameters)
{
    FFatalPresentationWorld F; if (!TestNotNull(TEXT("Real initialized persistent ASC"), F.ASC)) { return false; }
    F.Kill();
    TestTrue(TEXT("Native fatal recovery owns this actual death"), F.Pawn->GetRecoveryComponent()->OwnsFatalRecovery());
    TestTrue(TEXT("Native dispatch reserved the presentation before the Blueprint delay"), FSovFatalPresentationTestAccess::HasPending(F.PC));
    const uint64 OldSerial = FSovFatalPresentationTestAccess::Serial(F.PC);
    TestEqual(TEXT("Owned fatal never entered legacy death presentation"), F.PC->LegacyDeaths, 0);
    if (!TestTrue(TEXT("Real checkpoint resources revive the same pawn"), F.Restore())) { return false; }
    F.Advance(2.f); FSovFatalPresentationTestAccess::Poll(F.PC, OldSerial);
    TestTrue(TEXT("Restored pawn remains alive"), F.Pawn->IsAlive());
    TestFalse(TEXT("Revive retired its exact pending presentation"), FSovFatalPresentationTestAccess::HasPending(F.PC));
    TestEqual(TEXT("Late presentation cannot open any failure menu"), F.HUD->OpenCount, 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFatalPresentationFailureTest,
    "ProjectVelkorran.Campaign.Recovery.FailedNativeRetryRetainsActionableMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFatalPresentationFailureTest::RunTest(const FString& Parameters)
{
    FFatalPresentationWorld F; if (!TestNotNull(TEXT("Real initialized persistent ASC"), F.ASC)) { return false; }
    F.Kill(); F.Advance(2.5f);
    TestEqual(TEXT("No checkpoint produces the actual native failed state"), F.Pawn->GetRecoveryComponent()->GetRecoveryState(), ESovRecoveryState::Failed);
    TestEqual(TEXT("Failure opens the configured existing menu once"), F.HUD->OpenCount, 1);
    if (!TestNotNull(TEXT("Existing HUD returned the failure menu"), F.HUD->LastMenu.Get())) { return false; }
    TestTrue(TEXT("Failed recovery leaves an active menu"), F.HUD->LastMenu->IsActivated());
    F.Advance(.3f); TestEqual(TEXT("Polling cannot duplicate an owned menu"), F.HUD->OpenCount, 1);
    if (!TestTrue(TEXT("Later real restoration still succeeds"), F.Restore())) { return false; }
    TestFalse(TEXT("Revive closes only the exact owned failure menu"), F.HUD->LastMenu->IsActivated());
    TestEqual(TEXT("Native failure never queues the legacy timer"), F.PC->LegacyDeaths, 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFatalPresentationOwnershipTest,
    "ProjectVelkorran.Campaign.Recovery.FatalMenuFencesAvatarAndWidgetCallbacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFatalPresentationOwnershipTest::RunTest(const FString& Parameters)
{
    {
        FFatalPresentationWorld F; if (!TestNotNull(TEXT("Real initialized persistent ASC"), F.ASC)) { return false; }
        F.Kill();
        auto* Replacement = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
        F.ASC->InitAbilityActorInfo(F.PS, Replacement);
        F.Advance(2.f);
        TestEqual(TEXT("Old avatar cannot publish a delayed menu into a replacement"), F.HUD->OpenCount, 0);
        TestFalse(TEXT("Replaced actor-info epoch retires the old request"), FSovFatalPresentationTestAccess::HasPending(F.PC));
    }
    {
        FFatalPresentationWorld F; if (!TestNotNull(TEXT("Reentry ASC initialized"), F.ASC)) { return false; }
        bool bRestored = false;
        F.HUD->OnOpen = [&]() { bRestored = F.Restore(); };
        F.Kill(); F.Advance(2.5f);
        TestTrue(TEXT("Outward widget callback performed actual resource restoration"), bRestored);
        TestEqual(TEXT("Only the old context attempted menu creation"), F.HUD->OpenCount, 1);
        TestTrue(TEXT("Pawn remains alive after the callback"), F.Pawn->IsAlive());
        TestTrue(TEXT("Returned stale widget is deactivated after revalidation"), F.HUD->LastMenu && !F.HUD->LastMenu->IsActivated());
        TestFalse(TEXT("No stale presentation survives widget reentry"), FSovFatalPresentationTestAccess::HasPending(F.PC));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFatalPresentationOptOutTest,
    "ProjectVelkorran.Campaign.Recovery.UnconfiguredControllerPreservesStockDeathRoute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFatalPresentationOptOutTest::RunTest(const FString& Parameters)
{
    FFatalPresentationWorld F; if (!TestNotNull(TEXT("Real initialized persistent ASC"), F.ASC)) { return false; }
    F.PC->ConfigureFailureMenu(false); F.Kill();
    TestEqual(TEXT("Unconfigured integration retains stock Blueprint death dispatch"), F.PC->LegacyDeaths, 1);
    TestFalse(TEXT("Unconfigured controller reserves no native presentation"), FSovFatalPresentationTestAccess::HasPending(F.PC));
    TestEqual(TEXT("No new failure-menu request is invented"), F.HUD->OpenCount, 0);
    return true;
}
#endif
