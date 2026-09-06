// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovSaveSubsystem.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/SovCampaignGameMode.h"
#include "Framework/SovPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeSave.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace SovMissionTravelRecovery
{
    constexpr double TimeoutSeconds = 120.0;
    FGuid RequestFrom(const FString& Options, const TCHAR* Key)
    {
        FGuid Result;
        FGuid::ParseExact(UGameplayStatics::ParseOption(Options, Key), EGuidFormats::Digits, Result);
        return Result;
    }
}

void USovSaveSubsystem::InitializeMissionTravelRecovery()
{
    if (!GEngine) { return; }
    MissionTravelFailureHandle = GEngine->OnTravelFailure().AddWeakLambda(this,
        [this](UWorld* World, ETravelFailure::Type, const FString& Error) { RecordMissionTravelFailure(World, Error); });
    MissionNetworkFailureHandle = GEngine->OnNetworkFailure().AddWeakLambda(this,
        [this](UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString& Error) { RecordMissionTravelFailure(World, Error); });
}

void USovSaveSubsystem::DeinitializeMissionTravelRecovery()
{
    if (GEngine)
    {
        GEngine->OnTravelFailure().Remove(MissionTravelFailureHandle);
        GEngine->OnNetworkFailure().Remove(MissionNetworkFailureHandle);
    }
    MissionTravelFailureHandle.Reset(); MissionNetworkFailureHandle.Reset();
    ResetMissionTravelRecovery();
}

void USovSaveSubsystem::ResetMissionTravelRecovery()
{
    ResetRestoreOwner();
    MissionTravelOrigin = nullptr; MissionTravelNarrative = nullptr;
    MissionTravelOwner = FOperationOwner(); MissionTravelRequest.Invalidate(); MissionRecoveryRequest.Invalidate();
    MissionTravelSourceWorld.Reset(); MissionTravelDestinationWorld.Reset();
    MissionTravelDestinationId = NAME_None; MissionTravelDestinationDefinition.Reset(); MissionTravelDestinationMap.Reset();
    MissionTravelDeadline = 0; bMissionTravelFailurePending = false; bMissionRecoveryAttempted = false; MissionTravelError.Reset();
}

bool USovSaveSubsystem::ArmMissionTravelRecovery(ASovPlayerController* Source, USovCampaignDefinition* Destination,
    FGuid& Request, FString& Error)
{
    Request.Invalidate(); Error.Reset();
    if (bBusy || PendingSave || MissionTravelRequest.IsValid() || !IsValid(Source) || !IsValid(Destination)
        || Source->GetGameInstance() != GetGameInstance() || Source->GetNetMode() != NM_Standalone
        || !Source->GetCampaignState() || !Source->GetCampaignState()->GetActiveMission())
    { Error = TEXT("A valid, idle campaign travel owner is required."); return false; }
    TGuardValue<bool> Busy(bBusy, true);
    TStrongObjectPtr<ASovPlayerController> KeepSource(Source);
    TStrongObjectPtr<USovCampaignDefinition> KeepDestination(Destination);
    UWorld* const SourceWorld = Source->GetWorld();
    APawn* const SourcePawn = Source->GetPawn();
    USovCampaignDefinition* const OriginMission = Source->GetCampaignState()->GetActiveMission();
    TStrongObjectPtr<USovCampaignDefinition> KeepOriginMission(OriginMission);
    TStrongObjectPtr<USovCampaignStateComponent> KeepCampaign(Source->GetCampaignState());
    const FOperationOwner Owner = CaptureOperationOwner();
    if (!IsOperationOwnerCurrent(Owner, Error)) { return false; }
    int32 Bank = INDEX_NONE; bool bDamaged = false;
    TStrongObjectPtr<USovCampaignSaveGame> Origin(ReadBest(ESovSaveSlotKind::Checkpoint, 0, Bank, bDamaged, Error, &Owner));
    // A damaged newest bank needs an explicit recovery-save choice, not an implicit rollback.
    if (!Origin.IsValid() || bDamaged || !ValidateEnvelope(Origin.Get(), true, Error, &Owner)
        || Origin->Header.MissionId != OriginMission->MissionId
        || Origin->Header.MissionDefinition != FSoftObjectPath(OriginMission))
    { if (Error.IsEmpty()) { Error = TEXT("Mission travel needs a verified checkpoint for the current mission."); } return false; }
    TStrongObjectPtr<UNarrativeSave> Narrative(DecodeNarrative(Origin.Get(), Error, &Owner));
    if (!Narrative.IsValid() || !IsOperationOwnerCurrent(Owner, Error) || !IsValid(Source)
        || Source->IsActorBeingDestroyed() || Source->GetWorld() != SourceWorld || Source->GetPawn() != SourcePawn
        || Source->GetCampaignState() != KeepCampaign.Get() || !IsValid(KeepCampaign.Get())
        || KeepCampaign->GetActiveMission() != OriginMission || !IsValid(Destination)) { return false; }
    ResetMissionTravelRecovery();
    MissionTravelOrigin = Origin.Get(); MissionTravelNarrative = Narrative.Get(); MissionTravelOwner = Owner;
    MissionTravelSourceWorld = SourceWorld; MissionTravelDestinationId = Destination->MissionId;
    MissionTravelDestinationDefinition = FSoftObjectPath(Destination);
    MissionTravelDestinationMap = Destination->Map.ToSoftObjectPath().GetLongPackageName();
    MissionTravelRequest = FGuid::NewGuid(); MissionTravelDeadline = FPlatformTime::Seconds() + SovMissionTravelRecovery::TimeoutSeconds;
    Request = MissionTravelRequest;
    Source->PendingTravelOperationId = Request;
    Source->PendingTravelOriginGeneration = Origin->Header.Generation;
    return true;
}

bool USovSaveSubsystem::ValidateRestoredTravelIdentity(ASovPlayerController* PC) const
{
    return PC && MissionTravelRequest.IsValid() && !bMissionRecoveryAttempted && MissionTravelOrigin
        && PC->PendingTravelOperationId == MissionTravelRequest
        && PC->PendingTravelOriginGeneration == MissionTravelOrigin->Header.Generation;
}

void USovSaveSubsystem::CancelMissionTravelRecovery(const FGuid& Request)
{
    if (Request.IsValid() && Request == MissionTravelRequest && !bMissionRecoveryAttempted) { ResetMissionTravelRecovery(); }
}

bool USovSaveSubsystem::OwnsMissionTravelRequest(const FGuid& Request, FString& Error) const
{
    return Request.IsValid() && Request == MissionTravelRequest && !bMissionRecoveryAttempted
        && IsOperationOwnerCurrent(MissionTravelOwner, Error);
}

TFunction<bool()> USovSaveSubsystem::CaptureMissionTravelStorageFence(const FGuid& Request) const
{
    const FOperationOwner Owner = CaptureOperationOwner();
    const TWeakObjectPtr<const USovSaveSubsystem> Weak(this);
    return [Weak, Owner, Request]()
    {
        const auto* Self = Weak.Get(); FString Error;
        return Self && Request.IsValid() && Self->MissionTravelRequest == Request && !Self->bMissionRecoveryAttempted
            && Self->IsRetainedOwnerCurrent(Self->MissionTravelOwner, Error) && Self->IsOperationOwnerCurrent(Owner, Error);
    };
}

bool USovSaveSubsystem::ValidateMissionTravelWorld(UWorld& World, FString& Error)
{
    const auto* Mode = World.GetAuthGameMode<ASovCampaignGameMode>();
    if (!Mode || !UGameplayStatics::HasOption(Mode->OptionsString, TEXT("SovCampaignTransition"))) { return true; }
    const FGuid Request = SovMissionTravelRecovery::RequestFrom(Mode->OptionsString, TEXT("SovMissionTravelRequest"));
    if (World.GetGameInstance() != GetGameInstance() || !Request.IsValid() || Request != MissionTravelRequest
        || bMissionRecoveryAttempted || UGameplayStatics::HasOption(Mode->OptionsString, TEXT("SovCampaignSlotLoad")))
    { Error = TEXT("The mission travel request expired; select a verified recovery save."); return false; }
    MissionTravelDestinationWorld = &World;
    if (!IsRetainedOwnerCurrent(MissionTravelOwner, Error) || bPlatformSuspended || !Mode->InitialMission
        || Mode->InitialMission->MissionId != MissionTravelDestinationId
        || FSoftObjectPath(Mode->InitialMission) != MissionTravelDestinationDefinition
        || Mode->InitialMission->Map.ToSoftObjectPath().GetLongPackageName() != MissionTravelDestinationMap
        || FPackageName::GetLongPackagePath(World.GetOutermost()->GetName()) + TEXT("/")
            + UGameplayStatics::GetCurrentLevelName(&World, true) != MissionTravelDestinationMap)
    {
        if (Error.IsEmpty()) { Error = TEXT("The destination does not match the accepted mission travel transaction."); }
        bMissionTravelFailurePending = true; MissionTravelError = Error;
        return false;
    }
    return true;
}

void USovSaveSubsystem::RecordMissionTravelFailure(UWorld* World, const FString& Error)
{
    RecordMissionTravelFailureForRequest(MissionTravelRequest, World, Error);
}

void USovSaveSubsystem::RecordMissionTravelFailureForRequest(const FGuid& Request, UWorld* World, const FString& Error)
{
    if (!Request.IsValid() || Request != MissionTravelRequest) { return; }
    if (!MissionTravelRequest.IsValid() || !World || World->GetGameInstance() != GetGameInstance()) { return; }
    const auto* Mode = World->GetAuthGameMode<ASovCampaignGameMode>();
    const FString Options = Mode ? Mode->OptionsString : World->URL.ToString();
    const bool bRecoveryWorld = MissionRecoveryRequest.IsValid()
        && SovMissionTravelRecovery::RequestFrom(Options, TEXT("SovCampaignLoadRequest")) == MissionRecoveryRequest;
    const bool bDestinationWorld = SovMissionTravelRecovery::RequestFrom(Options, TEXT("SovMissionTravelRequest")) == MissionTravelRequest;
    if (World != MissionTravelSourceWorld.Get() && World != MissionTravelDestinationWorld.Get()
        && !bRecoveryWorld && !bDestinationWorld) { return; }
    // Engine callbacks only record failure. Never start another world load inside engine teardown.
    if (bMissionRecoveryAttempted && PendingLoadRequest == MissionRecoveryRequest)
    {
        // The failed source world can emit late teardown errors after recovery starts.
        // Without the recovery GUID (or its admitted world), leave the new load alone;
        // an uncorrelated engine failure is still bounded by the pending-load watchdog.
        if (!bRecoveryWorld && World != PendingDestination.Get()) { return; }
        bPendingLoadFailed = true; PendingLoadError = Error;
    }
    else if (!bMissionRecoveryAttempted)
    { bMissionTravelFailurePending = true; MissionTravelError = Error; }
}

void USovSaveSubsystem::NotifyMissionTravelReady(ASovPlayerController* PC, bool bSucceeded)
{
    if (!MissionTravelRequest.IsValid() || bMissionRecoveryAttempted || !IsValid(PC)
        || PC->GetGameInstance() != GetGameInstance()) { return; }
    UWorld* World = PC->GetWorld();
    const auto* Mode = World ? World->GetAuthGameMode<ASovCampaignGameMode>() : nullptr;
    if (!Mode || SovMissionTravelRecovery::RequestFrom(Mode->OptionsString, TEXT("SovMissionTravelRequest")) != MissionTravelRequest) { return; }
    FString Error;
    const auto* Player = Cast<ASovPlayerCharacterBase>(PC->GetPawn());
    if (bSucceeded && ValidateMissionTravelWorld(*World, Error) && Player && Player->IsCharacterReady()
        && PC->GetCampaignState() && PC->GetCampaignState()->GetActiveMission() == Mode->InitialMission)
    { ResetMissionTravelRecovery(); return; }
    bMissionTravelFailurePending = true;
    MissionTravelError = Error.IsEmpty() ? TEXT("The destination failed campaign readiness; restoring the verified origin.") : Error;
}

bool USovSaveSubsystem::BeginMissionOriginRecovery(FString& Error)
{
    UWorld* World = GetWorld();
    if (!World || bBusy || PendingSave || !MissionTravelRequest.IsValid() || bMissionRecoveryAttempted
        || !MissionTravelOrigin || !MissionTravelNarrative
        || !IsRetainedOwnerCurrent(MissionTravelOwner, Error) || bPlatformSuspended)
    { if (Error.IsEmpty()) { Error = TEXT("Origin recovery cannot start until the original storage owner and world are available."); } return false; }
    TGuardValue<bool> Busy(bBusy, true);
    const FGuid Request = MissionTravelRequest;
    const FOperationOwner Owner = CaptureOperationOwner();
    TStrongObjectPtr<USovCampaignSaveGame> KeepOrigin(MissionTravelOrigin);
    TStrongObjectPtr<UNarrativeSave> KeepNarrative(MissionTravelNarrative);
    // Narrative installs its input as its mutable live SaveGame. A failed attempt must
    // not mutate the retained origin later used by explicit retry.
    TStrongObjectPtr<UNarrativeSave> Attempt(DuplicateObject<UNarrativeSave>(KeepNarrative.Get(), this));
    if (!Attempt.IsValid() || !IsOperationOwnerCurrent(Owner, Error) || Request != MissionTravelRequest
        || MissionTravelOrigin != KeepOrigin.Get() || MissionTravelNarrative != KeepNarrative.Get() || GetWorld() != World)
    { if (Error.IsEmpty()) { Error = TEXT("Recovery ownership changed while preparing the attempt snapshot."); } return false; }
    bMissionRecoveryAttempted = true; bMissionTravelFailurePending = false; MissionTravelDeadline = 0;
    ResetRestoreOwner();
    PendingSave = MissionTravelOrigin; PendingNarrative = Attempt.Get();
    PendingLoadOwner = CaptureOperationOwner(); PendingLoadRequest = FGuid::NewGuid(); MissionRecoveryRequest = PendingLoadRequest;
    PendingLoadDeadline = FPlatformTime::Seconds() + SovMissionTravelRecovery::TimeoutSeconds;
    PendingDestination.Reset(); RejectedLoadWorld.Reset(); PendingLoadError.Reset();
    bPendingWorldApplied = false; bPendingLoadFailed = false; PendingAutosaves.Reset();
    const FString Options = TEXT("SovCampaignSlotLoad=1?SovCampaignLoadRequest=") + PendingLoadRequest.ToString(EGuidFormats::Digits);
    StartMissionRecoveryTravel(*World, PendingSave->Header.MapPackage, Options);
    return true;
}

void USovSaveSubsystem::StartMissionRecoveryTravel(UWorld& World, const FString& MapPackage, const FString& Options)
{
    // OpenLevel does not require the controller that initiated the original travel to survive.
    UGameplayStatics::OpenLevel(&World, FName(*MapPackage), true, Options);
}

void USovSaveSubsystem::TickMissionTravelRecovery()
{
    if (!MissionTravelRequest.IsValid() || bMissionRecoveryAttempted || bBusy || bPlatformSuspended) { return; }
    FString Error;
    if (!IsRetainedOwnerCurrent(MissionTravelOwner, Error))
    { bMissionTravelFailurePending = true; MissionTravelError = Error; }
    if (!bMissionTravelFailurePending && FPlatformTime::Seconds() <= MissionTravelDeadline) { return; }
    const FGuid Request = MissionTravelRequest;
    if (BeginMissionOriginRecovery(Error) || Request != MissionTravelRequest) { return; }
    // No automatic retries after account loss or recovery admission failure. Retain the exact
    // origin, release active ownership before listeners, and require an explicit new decision.
    bMissionRecoveryAttempted = true; bMissionTravelFailurePending = false; MissionTravelRequest.Invalidate();
    MissionTravelDeadline = 0;
    const FSovSaveSlotHeader Header = MissionTravelOrigin ? MissionTravelOrigin->Header : FSovSaveSlotHeader();
    OnLoadCompleted.Broadcast(ESovSaveResult::RecoveryAvailable, Header, Error);
}

void USovSaveSubsystem::CompleteMissionTravelRecovery(bool bSucceeded, const FString& Error)
{
    if (!MissionRecoveryRequest.IsValid() || PendingLoadRequest != MissionRecoveryRequest) { return; }
    if (bSucceeded) { ResetMissionTravelRecovery(); return; }
    MissionRecoveryRequest.Invalidate(); MissionTravelRequest.Invalidate(); MissionTravelDeadline = 0;
    bMissionTravelFailurePending = false; MissionTravelError = Error;
    // Origin bytes stay pinned for an explicit retry; the ordinary pending-load completion
    // broadcasts the error once and never recursively requests another travel.
}

bool USovSaveSubsystem::RetryMissionTravelRecovery(FString& Error)
{
    Error.Reset();
    if (MissionTravelRequest.IsValid() || !bMissionRecoveryAttempted || !MissionTravelOrigin || !MissionTravelNarrative
        || bBusy || PendingSave || bPlatformSuspended || !IsPlatformStorageOwnerAvailable()
        || MissionTravelOwner.Namespace != AccountNamespace || MissionTravelOwner.LocalUser != UserIndex
        || MissionTravelOwner.SelectionEpoch != SelectionEpoch)
    { Error = TEXT("Explicit origin retry requires the original selected account and no active load."); return false; }
    MissionTravelOwner = CaptureOperationOwner(); MissionTravelRequest = FGuid::NewGuid();
    bMissionRecoveryAttempted = false;
    const FGuid Request = MissionTravelRequest;
    if (BeginMissionOriginRecovery(Error)) { return true; }
    if (MissionTravelRequest == Request) { MissionTravelRequest.Invalidate(); bMissionRecoveryAttempted = true; }
    return false;
}

bool USovSaveSubsystem::HasTravelRecovery() const
{
    return MissionTravelOrigin && MissionTravelNarrative
        && MissionTravelOwner.Namespace == AccountNamespace && MissionTravelOwner.LocalUser == UserIndex
        && MissionTravelOwner.SelectionEpoch == SelectionEpoch && IsPlatformStorageOwnerAvailable();
}
