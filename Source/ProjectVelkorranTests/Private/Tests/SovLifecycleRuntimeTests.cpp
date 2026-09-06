// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Framework/SovPlayerController.h"
#include "Framework/SovApplicationLifecycleComponent.h"
#include "Framework/SovApplicationLifecycleSubsystem.h"
#include "UI/SovApplicationInterruptionMenu.h"
#include "Save/SovSaveSubsystem.h"
#include "NarrativeGameplayTags.h"
#include "Tests/SovLifecycleTestFixtures.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SafeZone.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProperties.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovLifecycleTestAccess
{
    static bool ExternalPause(const ASovPlayerController* PC) { return PC->bExternalPauseRequested; }
    static int32 PauseOwners(const ASovPlayerController* PC) { return PC->SystemPauseOwners.Num(); }
    static void Change(USovApplicationLifecycleSubsystem* S, SovLifecyclePolicy::Reason R, bool bActive)
    { S->SetReason(R, bActive); }
    static void AttachSave(USovApplicationLifecycleSubsystem* S, USovSaveSubsystem* Save) { S->Saves = Save; }
    static bool HasPressedInput(const ASovPlayerController* PC, FGameplayTag Tag) { return PC->PressedAbilityInputTags.Contains(Tag); }
    static void Hold(USovApplicationLifecycleComponent* C) { C->State.SetReason(SovLifecyclePolicy::Reason::Controller, true, 1.); }
};
namespace
{
    struct FLifecycleWorld
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        UWorld* World = nullptr;
        ASovPlayerController* PC = nullptr;
        ASovLifecycleTestGameMode* Mode = nullptr;
        FLifecycleWorld()
        {
            const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
                .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &WorldInitialization, true);
            World->SetGameInstance(Instance.Get());
            if (GEngine)
            { auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World); Context.OwningGameInstance = Instance.Get(); }
            World->InitWorld(WorldInitialization);
            World->UpdateWorldComponents(!FPlatformProperties::RequiresCookedData(), false);
            FURL URL; URL.AddOption(*(TEXT("game=") + ASovLifecycleTestGameMode::StaticClass()->GetPathName()));
            if (World->SetGameMode(URL))
            {
                // Normal actor initialization creates GameState before InitGameState.
                // Calling InitGameState directly dereferences the absent state in UE 5.7.
                World->InitializeActorsForPlay(URL);
                Mode = World->GetAuthGameMode<ASovLifecycleTestGameMode>();
                PC = World->SpawnActor<ASovPlayerController>();
            }
        }
        ~FLifecycleWorld() { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSystemPauseOwnershipTest, "ProjectVelkorran.Platform.Lifecycle.SharedPauseOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSystemPauseOwnershipTest::RunTest(const FString&)
{
    FLifecycleWorld W;
    if (!W.PC || !W.Mode) { AddError(TEXT("Lifecycle world could not initialize")); return false; }
    const FGameplayTag Input = FNarrativeGameplayTags::Get().Narrative_Input_AltAttack;
    W.PC->AbilityInputPressed(Input);
    TestTrue(TEXT("Existing semantic input route accepts a normal press"), FSovLifecycleTestAccess::HasPressedInput(W.PC, Input));
    FSovLifecycleTestAccess::Hold(W.PC->GetApplicationLifecycle());
    W.PC->ReleaseHeldAbilityInputs(); W.PC->AbilityInputPressed(Input);
    TestFalse(TEXT("Interrupted gameplay cannot re-latch a released or late controller input"), FSovLifecycleTestAccess::HasPressedInput(W.PC, Input));
    TestTrue(TEXT("First boot acquires native pause"), W.PC->AcquireSystemPause(TEXT("AccessibilitySetup")));
    TestTrue(TEXT("Suspend can acquire an already paused world"), W.PC->AcquireSystemPause(TEXT("PlatformInterruption")));
    TestTrue(TEXT("Save failure can acquire while both reasons exist"), W.PC->AcquireSystemPause(TEXT("SaveFailure")));
    TestTrue(TEXT("Duplicate acquisition is idempotent"), W.PC->AcquireSystemPause(TEXT("SaveFailure")));
    TestEqual(TEXT("Exactly three owners exist"), FSovLifecycleTestAccess::PauseOwners(W.PC), 3);
    W.Mode->ClearPause();
    TestTrue(TEXT("GameMode unpause also respects the registered system ownership rules"), W.World->IsPaused());
    W.PC->ReleaseSystemPause(TEXT("AccessibilitySetup"));
    TestFalse(TEXT("A menu cannot unpause an active platform/save hold"), W.PC->SetPause(false));
    W.PC->ReleaseSystemPause(TEXT("PlatformInterruption"));
    TestTrue(TEXT("Save failure still owns simulation pause"), W.World->IsPaused());
    W.PC->ReleaseSystemPause(TEXT("SaveFailure"));
    TestFalse(TEXT("Last owned reason releases native pause"), W.World->IsPaused());
    TestTrue(TEXT("Existing authored menu pause succeeds"), W.PC->SetPause(true));
    W.PC->AcquireSystemPause(TEXT("PlatformInterruption")); W.PC->ReleaseSystemPause(TEXT("PlatformInterruption"));
    TestTrue(TEXT("Platform resume preserves an earlier menu pause"), W.World->IsPaused());
    W.PC->SetPause(false);
    W.PC->AcquireSystemPause(TEXT("PlatformInterruption")); W.PC->SetPause(true);
    W.PC->ReleaseSystemPause(TEXT("PlatformInterruption"));
    TestTrue(TEXT("Platform resume preserves a later menu pause"), W.World->IsPaused());
    W.PC->SetPause(false);
    TestTrue(TEXT("A fresh platform hold is acquired for an overlapping authored rule"), W.PC->AcquireSystemPause(TEXT("PlatformInterruption")));
    bool bMenuReadyToUnpause = false;
    TestTrue(TEXT("An already paused world accepts a later menu and its unpause rule"),
        W.PC->SetPause(true, FCanUnpause::CreateLambda([&bMenuReadyToUnpause]() { return bMenuReadyToUnpause; })));
    W.PC->ReleaseSystemPause(TEXT("PlatformInterruption"));
    W.PC->SetPause(false);
    TestTrue(TEXT("The later menu's unpause delegate remains authoritative after platform release"), W.World->IsPaused());
    bMenuReadyToUnpause = true;
    W.PC->SetPause(false);
    TestFalse(TEXT("The menu can release its own remaining rule when ready"), W.World->IsPaused());
    W.Mode->bAcceptPause = false;
    TestFalse(TEXT("GameMode can refuse a pause"), W.PC->SetPause(true));
    TestFalse(TEXT("Refused pause cannot leave a phantom menu owner"), FSovLifecycleTestAccess::ExternalPause(W.PC));
    TestFalse(TEXT("Refused system pause cannot claim success"), W.PC->AcquireSystemPause(TEXT("PlatformInterruption")));
    TestEqual(TEXT("Refused system pause retires its reason"), FSovLifecycleTestAccess::PauseOwners(W.PC), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLifecycleControllerReplacementTest, "ProjectVelkorran.Platform.Lifecycle.ControllerReplacementRetainsApplicationHold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovLifecycleControllerReplacementTest::RunTest(const FString&)
{
    using Reason = SovLifecyclePolicy::Reason;
    FLifecycleWorld W;
    auto* Application = NewObject<USovLifecycleTestApplicationSubsystem>(W.Instance.Get());
    auto* Saves = NewObject<USovSaveSubsystem>(W.Instance.Get());
    FSovLifecycleTestAccess::AttachSave(Application, Saves);
    FSovLifecycleTestAccess::Change(Application, Reason::Background, true);
    FSovLifecycleTestAccess::Change(Application, Reason::Background, true);
    FSovLifecycleTestAccess::Change(Application, Reason::Overlay, true);
    TestEqual(TEXT("Only the first unavailable transition rolls back an owned display preview"), Application->DisplayReversions, 1);
    TestTrue(TEXT("Actual save subsystem closes its storage gate on background"), Saves->IsPlatformStorageSuspended());
    TestTrue(TEXT("Application owns background hold without a viewport or a controller dependency"), Application->IsApplicationUnavailable());
    if (W.PC) { W.PC->Destroy(); W.PC = nullptr; }
    W.PC = W.World->SpawnActor<ASovPlayerController>();
    TestTrue(TEXT("Controller replacement retains the actual save gate"), Saves->IsPlatformStorageSuspended());
    TestTrue(TEXT("Controller replacement does not clear persistent application ownership"), Application->IsAwaitingResume());
    FSovLifecycleTestAccess::Change(Application, Reason::Background, false);
    TestTrue(TEXT("Overlapping overlay still holds the actual save gate"), Saves->IsPlatformStorageSuspended());
    FSovLifecycleTestAccess::Change(Application, Reason::Overlay, false);
    TestFalse(TEXT("Foreground reopens the actual save gate after replacement"), Saves->IsPlatformStorageSuspended());
    TestFalse(TEXT("Foreground removes the background reason across controller replacement"), Application->IsApplicationUnavailable());
    TestTrue(TEXT("Foreground still requires deliberate resume"), Application->IsAwaitingResume());
    Application->AcknowledgeResume();
    TestFalse(TEXT("Explicit acknowledgement clears the persistent resume hold"), Application->IsAwaitingResume());
    TestFalse(TEXT("Headless native world does not acquire a UI prerequisite"), W.PC->GetApplicationLifecycle()->CanResumeGameplay());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeInterruptionWidgetTest, "ProjectVelkorran.Platform.Lifecycle.NativeResumeSafeZone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeInterruptionWidgetTest::RunTest(const FString&)
{
    auto* Widget = NewObject<USovApplicationInterruptionMenu>(); Widget->Initialize(); Widget->TakeWidget();
    TestTrue(TEXT("Resume UI builds without editor-authored assets"), Widget->WidgetTree && Widget->WidgetTree->RootWidget);
    TestTrue(TEXT("Resume UI root respects TV safe zone"), Widget->WidgetTree && Cast<USafeZone>(Widget->WidgetTree->RootWidget));
    return true;
}
#endif
