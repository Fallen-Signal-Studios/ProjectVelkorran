// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Framework/SovApplicationLifecycleSubsystem.h"
#include "Framework/SovApplicationLifecycleComponent.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "Platform/SovPlatformServicesSubsystem.h"
#include "Save/SovSaveSubsystem.h"
#include "Settings/SovGameUserSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTime.h"
#include "Async/Async.h"

namespace
{
    using Reason = SovLifecyclePolicy::Reason;
    template<typename Callable> void DispatchApplication(TWeakObjectPtr<USovApplicationLifecycleSubsystem> Weak, Callable Work)
    {
        auto Dispatch = [Weak, Work]() { if (auto* Subsystem = Weak.Get()) { Work(*Subsystem); } };
        if (IsInGameThread()) { Dispatch(); } else { AsyncTask(ENamedThreads::GameThread, MoveTemp(Dispatch)); }
    }
}
void USovApplicationLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Saves = Collection.InitializeDependency<USovSaveSubsystem>();
    Collection.InitializeDependency<USovPlatformServicesSubsystem>();
    const TWeakObjectPtr<USovApplicationLifecycleSubsystem> Weak(this);
    BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddWeakLambda(this, [Weak]()
    { DispatchApplication(Weak, [](ThisClass& Self) { Self.SetReason(Reason::Background, true); }); });
    ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddWeakLambda(this, [Weak]()
    { DispatchApplication(Weak, [](ThisClass& Self) { Self.SetReason(Reason::Background, false); }); });
    DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddWeakLambda(this, [Weak]()
    { DispatchApplication(Weak, [](ThisClass& Self) { Self.SetReason(Reason::Inactive, true); }); });
    ReactivateHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddWeakLambda(this, [Weak]()
    { DispatchApplication(Weak, [](ThisClass& Self) { Self.SetReason(Reason::Inactive, false); }); });
    OverlayHandle = FCoreDelegates::ApplicationSystemUIOverlayStateChangedDelegate.AddWeakLambda(this, [Weak](bool bVisible)
    { DispatchApplication(Weak, [bVisible](ThisClass& Self) { Self.SetReason(Reason::Overlay, bVisible); }); });
}
void USovApplicationLifecycleSubsystem::SetReason(Reason Value, bool bActive)
{
    if (bEnding || State.HasReason(Value) == bActive) { return; }
    const uint64 Expected = ++Generation;
    const bool bWasUnavailable = State.IsApplicationUnavailable();
    State.SetReason(Value, bActive, FPlatformTime::Seconds());
    const bool bUnavailable = State.IsApplicationUnavailable();
    UGameInstance* GI = GetGameInstance(); if (!GI) { return; }
    // Close the storage gate before cancellation observers can attempt another operation.
    if (bUnavailable && Saves) { Saves->SetPlatformSuspended(true); }
    if (Expected != Generation || bEnding) { return; }
    if (!bWasUnavailable && bUnavailable)
    {
        RevertDisplayPreview();
        if (Expected != Generation || bEnding) { return; }
    }
    if (auto* Platform = GI->GetSubsystem<USovPlatformServicesSubsystem>())
    {
        if (!bActive) { Platform->RefreshPlatformAccount(); }
        if (Expected != Generation || bEnding) { return; }
        if (!bWasUnavailable && bUnavailable) { Platform->CancelCloudOperation(); }
    }
    if (Expected != Generation || bEnding) { return; }
    if (!bUnavailable)
    {
        // Revalidate the current identity while I/O is fenced, then allow a safe
        // frontend account selection to retry after its profile storage becomes available.
        if (Saves) { Saves->SetPlatformSuspended(false); }
        if (auto* Platform = GI->GetSubsystem<USovPlatformServicesSubsystem>()) { Platform->RefreshPlatformAccount(); }
        if (Expected != Generation || bEnding) { return; }
    }
    for (auto* Player : GI->GetLocalPlayers())
    {
        if (Player) { if (auto* Narrator = Player->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->SetApplicationSuspended(bUnavailable); } }
        if (Expected != Generation || bEnding) { return; }
    }
    if (auto* PC = GI->GetFirstLocalPlayerController())
    { if (auto* Component = PC->FindComponentByClass<USovApplicationLifecycleComponent>()) { Component->RefreshOwnership(); } }
}
void USovApplicationLifecycleSubsystem::AcknowledgeResume() { State.Resume(FPlatformTime::Seconds()); }
void USovApplicationLifecycleSubsystem::RevertDisplayPreview()
{ if (auto* Settings = USovGameUserSettings::Get()) { Settings->RevertUnconfirmedHDRPreview(); } }
void USovApplicationLifecycleSubsystem::Deinitialize()
{
    bEnding = true;
    ++Generation;
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundHandle);
    FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
    FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(ReactivateHandle);
    FCoreDelegates::ApplicationSystemUIOverlayStateChangedDelegate.Remove(OverlayHandle);
    Saves = nullptr;
    Super::Deinitialize();
}
