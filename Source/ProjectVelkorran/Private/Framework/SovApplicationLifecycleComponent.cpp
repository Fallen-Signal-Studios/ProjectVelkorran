// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Framework/SovApplicationLifecycleComponent.h"
#include "Framework/SovApplicationLifecycleSubsystem.h"
#include "Framework/SovPlayerController.h"
#include "UI/SovApplicationInterruptionMenu.h"
#include "Feedback/SovHapticFeedbackComponent.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "Platform/SovPlatformServicesSubsystem.h"
#include "Save/SovSaveSubsystem.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "NarrativeGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "Framework/Application/SlateApplication.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "SovApplicationLifecycle"
namespace
{
    const FName LifecyclePause(TEXT("PlatformInterruption"));
    using Reason = SovLifecyclePolicy::Reason;
    // Core platform callbacks normally arrive on the game thread. Never mutate UObjects
    // from a platform callback thread; the platform integration must drain game-thread
    // lifecycle work before actually suspending the process.
    template<typename Callable> void DispatchLifecycle(TWeakObjectPtr<USovApplicationLifecycleComponent> Weak, Callable Work)
    {
        auto Dispatch = [Weak, Work]() { if (auto* Component = Weak.Get()) { Work(*Component); } };
        if (IsInGameThread()) { Dispatch(); } else { AsyncTask(ENamedThreads::GameThread, MoveTemp(Dispatch)); }
    }
}

USovApplicationLifecycleComponent::USovApplicationLifecycleComponent()
{
    PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.bTickEvenWhenPaused = true;
    PrimaryComponentTick.TickInterval = 0.f; bAutoActivate = true;
}
bool USovApplicationLifecycleComponent::HasLocalViewport() const
{
    const auto* PC = Cast<ASovPlayerController>(GetOwner());
    const auto* Player = PC ? PC->GetLocalPlayer() : nullptr;
    return !bEnding && !IsRunningCommandlet() && FSlateApplication::IsInitialized() && PC && PC->IsLocalController()
        && Player && Player->ViewportClient && Player->ViewportClient->Viewport && GetNetMode() == NM_Standalone;
}
void USovApplicationLifecycleComponent::BeginPlay() { Super::BeginPlay(); RefreshOwnership(); }
void USovApplicationLifecycleComponent::RefreshOwnership()
{
    if (!HasLocalViewport() || bRefreshingOwnership) { return; }
    TGuardValue<bool> RefreshGuard(bRefreshingOwnership, true);
    auto* PC = CastChecked<ASovPlayerController>(GetOwner());
    if (auto* GI = PC->GetGameInstance())
    {
        if (auto* Application = GI->GetSubsystem<USovApplicationLifecycleSubsystem>())
        {
            for (const auto Value : { Reason::Background, Reason::Inactive, Reason::Overlay })
            { SetReason(Value, Application->HasReason(Value)); }
            // Controller replacement may occur after foreground but before explicit resume.
            if (Application->IsAwaitingResume() && !State.IsInterrupted())
            { SetReason(Reason::Background, true); SetReason(Reason::Background, false); }
        }
    }
    auto& Mapper = IPlatformInputDeviceMapper::Get();
    const auto User = PC->GetLocalPlayer()->GetPlatformUserId();
    if (!bBound)
    {
        bBound = true; ObservedUser = User;
        const TWeakObjectPtr<USovApplicationLifecycleComponent> Weak(this);
        ConnectionHandle = Mapper.GetOnInputDeviceConnectionChange().AddWeakLambda(this,
            [Weak](EInputDeviceConnectionState Connection, FPlatformUserId ChangedUser, FInputDeviceId Device)
        {
            DispatchLifecycle(Weak, [Connection, ChangedUser, Device](ThisClass& Self)
            {
                // Disconnect can arrive after the mapper has cleared its user association.
                // Retain the last owned device IDs instead of trusting ChangedUser alone.
                if (ChangedUser == Self.ObservedUser || Self.KnownOwnedDevices.Contains(Device))
                {
                    if (Connection == EInputDeviceConnectionState::Disconnected)
                    { Self.SetReason(Reason::Controller, true); Self.KnownOwnedDevices.Remove(Device); }
                    Self.RefreshPlatformServices(); Self.RefreshOwnership();
                }
            });
        });
        PairingHandle = Mapper.GetOnInputDevicePairingChange().AddWeakLambda(this,
            [Weak](FInputDeviceId Device, FPlatformUserId NewUser, FPlatformUserId OldUser)
        {
            DispatchLifecycle(Weak, [Device, NewUser, OldUser](ThisClass& Self)
            {
                if (OldUser == Self.ObservedUser || NewUser == Self.ObservedUser || Self.KnownOwnedDevices.Contains(Device))
                {
                    Self.SetReason(Reason::Controller, true);
                    if (NewUser != Self.ObservedUser) { Self.KnownOwnedDevices.Remove(Device); }
                    Self.RefreshPlatformServices(); Self.RefreshOwnership();
                }
            });
        });
    }
    if (User != ObservedUser)
    {
        SetReason(Reason::Account, true); ObservedUser = User; KnownOwnedDevices.Reset(); RefreshPlatformServices();
    }
    TArray<FInputDeviceId> Devices;
    if (User.IsValid()) { Mapper.GetAllInputDevicesForUser(User, Devices); }
    for (const auto Device : Devices)
    {
        if (Mapper.GetInputDeviceConnectionState(Device) == EInputDeviceConnectionState::Connected)
        { KnownOwnedDevices.Add(Device); }
    }
    if (!FPlatformProperties::SupportsWindowedMode() || State.HasReason(Reason::Controller))
    { SetReason(Reason::Controller, !HasOwningInputDevice()); }
    const bool bAvailable = HasStorageOwner();
    if (bAvailable) { bHadStorageOwner = true; }
    if (bHadStorageOwner || !FPlatformProperties::SupportsWindowedMode()) { SetReason(Reason::Account, !bAvailable); }
    ApplyInterruption();
}
bool USovApplicationLifecycleComponent::HasOwningInputDevice() const
{
    const auto* PC = Cast<APlayerController>(GetOwner()); const auto* Player = PC ? PC->GetLocalPlayer() : nullptr;
    if (!Player || !Player->GetPlatformUserId().IsValid()) { return false; }
    auto& Mapper = IPlatformInputDeviceMapper::Get(); TArray<FInputDeviceId> Devices;
    Mapper.GetAllInputDevicesForUser(Player->GetPlatformUserId(), Devices);
    for (const auto Device : Devices)
    { if (Mapper.GetInputDeviceConnectionState(Device) == EInputDeviceConnectionState::Connected) { return true; } }
    return false;
}
bool USovApplicationLifecycleComponent::HasStorageOwner() const
{
    const auto* PC = Cast<APlayerController>(GetOwner()); const auto* GI = PC ? PC->GetGameInstance() : nullptr;
    const auto* Save = GI ? GI->GetSubsystem<USovSaveSubsystem>() : nullptr;
    return Save && Save->IsPlatformStorageOwnerAvailable();
}
double USovApplicationLifecycleComponent::ActiveTimeSeconds(const APlayerController* Player)
{
    const double Now = FPlatformTime::Seconds();
    const auto* Component = Player ? Player->FindComponentByClass<USovApplicationLifecycleComponent>() : nullptr;
    return Component ? Component->State.ActiveTime(Now) : Now;
}
void USovApplicationLifecycleComponent::SetReason(Reason Value, bool bActive)
{
    if (bEnding || State.HasReason(Value) == bActive) { return; }
    State.SetReason(Value, bActive, FPlatformTime::Seconds());
    ApplyInterruption();
}
void USovApplicationLifecycleComponent::ApplyInterruption()
{
    if (bApplyingInterruption || bEnding || !HasLocalViewport()) { return; }
    TGuardValue<bool> Guard(bApplyingInterruption, true);
    auto* PC = CastChecked<ASovPlayerController>(GetOwner());
    if (!State.IsInterrupted())
    {
        if (bOwnInputLock) { bOwnInputLock = false; PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); }
        if (bOwnPause) { bOwnPause = false; PC->ReleaseSystemPause(LifecyclePause); }
        if (InterruptionMenu) { auto* Menu = InterruptionMenu.Get(); InterruptionMenu = nullptr; Menu->DeactivateWidget(); }
        return;
    }
    if (!bOwnInputLock)
    {
        bOwnInputLock = true; PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
        PC->ReleaseHeldAbilityInputs();
        if (PC->PlayerInput) { PC->PlayerInput->FlushPressedKeys(); }
    }
    if (auto* Haptics = PC->GetHapticFeedback()) { Haptics->CancelAllFeedback(); }
    // Only pawn initialization/restore orchestration needs world timers. The source world
    // remains a playable simulation during accepted map travel and must stay paused.
    const auto Transition = PC->GetCampaignTransitionState();
    const bool bCanPause = Transition == ESovCampaignTransitionState::Idle || Transition == ESovCampaignTransitionState::Failed
        || Transition == ESovCampaignTransitionState::Travelling;
    if (bCanPause && !bOwnPause) { bOwnPause = PC->AcquireSystemPause(LifecyclePause); }
    else if (!bCanPause && bOwnPause) { bOwnPause = false; PC->ReleaseSystemPause(LifecyclePause); }
    if (!IsApplicationUnavailable() && bCanPause && PC->GetNarrativeGameplayHUD()
        && (!InterruptionMenu || !InterruptionMenu->IsActivated()))
    {
        InterruptionMenu = Cast<USovApplicationInterruptionMenu>(PC->GetNarrativeGameplayHUD()->OpenMenu(
            USovApplicationInterruptionMenu::StaticClass(), FNarrativeGameplayTags::Get().UI_Layer_Modal));
    }
}
bool USovApplicationLifecycleComponent::CanResumeGameplay() const
{
    const auto* PC = Cast<ASovPlayerController>(GetOwner());
    return HasLocalViewport() && PC
        && (PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle || PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Failed)
        && State.CanResume() && HasStorageOwner()
        && (FPlatformProperties::SupportsWindowedMode() || HasOwningInputDevice());
}
bool USovApplicationLifecycleComponent::ResumeGameplay()
{
    RefreshPlatformServices(); RefreshOwnership();
    if (!CanResumeGameplay()) { return false; }
    auto* PC = CastChecked<ASovPlayerController>(GetOwner());
    // Confirmation cannot replay an input held before suspend or a trigger used to dismiss a system screen.
    PC->ReleaseHeldAbilityInputs(); if (PC->PlayerInput) { PC->PlayerInput->FlushPressedKeys(); }
    if (!CanResumeGameplay() || !State.Resume(FPlatformTime::Seconds())) { return false; }
    if (auto* GI = PC->GetGameInstance())
    { if (auto* Application = GI->GetSubsystem<USovApplicationLifecycleSubsystem>()) { Application->AcknowledgeResume(); } }
    ApplyInterruption(); return true;
}
FText USovApplicationLifecycleComponent::GetInterruptionMessage() const
{
    if (IsApplicationUnavailable()) { return LOCTEXT("Background", "Game paused while a system screen is open."); }
    if (State.HasReason(Reason::Account) || !HasStorageOwner())
    { return LOCTEXT("Account", "Reconnect the account that owns this game session. Your existing checkpoints are retained."); }
    if (State.HasReason(Reason::Controller))
    { return LOCTEXT("Controller", "Connect a controller assigned to this player, then select Resume game."); }
    if (const auto* PC = Cast<ASovPlayerController>(GetOwner()))
    { if (PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Failed)
      { return LOCTEXT("Recovery", "Select Resume game to return to campaign recovery. The interrupted transition has not completed."); } }
    return LOCTEXT("Resume", "Game paused. Select Resume game when you are ready.");
}
void USovApplicationLifecycleComponent::RefreshPlatformServices()
{
    if (bEnding) { return; }
    if (const auto* PC = Cast<APlayerController>(GetOwner()))
    { if (auto* GI = PC->GetGameInstance()) { if (auto* Platform = GI->GetSubsystem<USovPlatformServicesSubsystem>()) { Platform->RefreshPlatformAccount(); } } }
}
void USovApplicationLifecycleComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick)
{ Super::TickComponent(Delta, TickType, Tick); RefreshOwnership(); }
void USovApplicationLifecycleComponent::EndPlay(const EEndPlayReason::Type ReasonValue)
{
    bEnding = true;
    if (bBound)
    {
        auto& Mapper = IPlatformInputDeviceMapper::Get();
        Mapper.GetOnInputDeviceConnectionChange().Remove(ConnectionHandle); Mapper.GetOnInputDevicePairingChange().Remove(PairingHandle);
    }
    if (auto* PC = Cast<ASovPlayerController>(GetOwner()))
    {
        if (bOwnInputLock) { bOwnInputLock = false; PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); }
        if (bOwnPause) { bOwnPause = false; PC->ReleaseSystemPause(LifecyclePause); }
    }
    if (InterruptionMenu) { InterruptionMenu->DeactivateWidget(); InterruptionMenu = nullptr; }
    Super::EndPlay(ReasonValue);
}
#undef LOCTEXT_NAMESPACE
