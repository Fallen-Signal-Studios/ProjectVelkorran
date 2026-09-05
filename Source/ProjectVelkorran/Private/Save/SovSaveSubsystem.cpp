// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovSaveSubsystem.h"
#include "Save/SovSavePolicy.h"
#include "Platform/SovPlatformServicesAdapter.h"
#include "Platform/SovPlatformServicesSubsystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovCampaignGameMode.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Misc/Crc.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProperties.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "NarrativeGameplayTags.h"
#include "Progression/SovTechniqueComponent.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "SaveSystemDeveloperSettings.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "UObject/StrongObjectPtr.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
    class FPlatformSaveStorage final : public ISovSaveStorage
    {
    public:
        bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) override
        { return UGameplayStatics::LoadDataFromSlot(Bytes, Slot, User); }
        bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) override
        { return UGameplayStatics::SaveDataToSlot(Bytes, Slot, User); }
        bool Exists(const FString& Slot, int32 User) override
        { return UGameplayStatics::DoesSaveGameExist(Slot, User); }
        bool Remove(const FString& Slot, int32 User) override
        { return UGameplayStatics::DeleteGameInSlot(Slot, User); }
    };
    constexpr double LoadTimeoutSeconds = 120.0;
    SovSavePolicy::Kind PolicyKind(ESovSaveSlotKind Kind) { return static_cast<SovSavePolicy::Kind>(Kind); }
    constexpr uint32 ProfileHintMagic = 0x534F5641;
    constexpr int32 ProfileHintBytes = 88;
    struct FProfileHint { FString Namespace; int64 Generation = 0; };
    FString ProfileHintSlot(int32 User, int32 Bank)
    { return FString::Printf(TEXT("SovAccount_v1_%s_%d_%c"), FPlatformProperties::IniPlatformName(), User, Bank ? TCHAR('B') : TCHAR('A')); }
    bool IsOpaqueNamespace(const FString& Value)
    {
        if (Value.Len() != 32) { return false; }
        for (const TCHAR Character : Value)
        { if (!((Character >= TCHAR('0') && Character <= TCHAR('9')) || (Character >= TCHAR('a') && Character <= TCHAR('f')))) { return false; } }
        return true;
    }
    bool DecodeProfileHint(const TArray<uint8>& Bytes, int32 ExpectedUser, FProfileHint& Hint)
    {
        if (Bytes.Num() != ProfileHintBytes) { return false; }
        FMemoryReader Reader(Bytes, true); uint32 StoredCRC = 0;
        Reader.Seek(ProfileHintBytes - sizeof(uint32)); Reader << StoredCRC;
        if (StoredCRC != FCrc::MemCrc32(Bytes.GetData(), ProfileHintBytes - sizeof(uint32))) { return false; }
        Reader.Seek(0); uint32 Magic = 0, Version = 0; int32 User = -1; int64 Generation = 0;
        ANSICHAR Namespace[33] = {}, Platform[33] = {};
        Reader << Magic << Version << User << Generation; Reader.Serialize(Namespace, 32); Reader.Serialize(Platform, 32);
        Hint.Namespace = ANSI_TO_TCHAR(Namespace); Hint.Generation = Generation;
        return !Reader.IsError() && Magic == ProfileHintMagic && Version == 1 && User == ExpectedUser && User >= 0
            && Generation > 0 && IsOpaqueNamespace(Hint.Namespace)
            && FString(ANSI_TO_TCHAR(Platform)) == FMD5::HashAnsiString(FPlatformProperties::IniPlatformName());
    }
    TArray<uint8> EncodeProfileHint(const FString& Namespace, int32 User, int64 Generation)
    {
        TArray<uint8> Bytes; FMemoryWriter Writer(Bytes, true); uint32 Magic = ProfileHintMagic, Version = 1;
        Writer << Magic << Version << User << Generation;
        ANSICHAR Hash[32], Platform[32]; const FString PlatformHash = FMD5::HashAnsiString(FPlatformProperties::IniPlatformName());
        for (int32 Index = 0; Index < 32; ++Index) { Hash[Index] = static_cast<ANSICHAR>(Namespace[Index]); Platform[Index] = static_cast<ANSICHAR>(PlatformHash[Index]); }
        Writer.Serialize(Hash, 32); Writer.Serialize(Platform, 32);
        uint32 CRC = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num()); Writer << CRC; return Bytes;
    }
}

void USovSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Storage = MakeUnique<FPlatformSaveStorage>();
    // Restore only a hash-only, versioned hint from this platform/local-user save compartment.
    // Offline startup must find the previous campaign even if network identity is temporarily absent.
    if (PLATFORM_DESKTOP)
    {
        AccountNamespace = FMD5::HashAnsiString(TEXT("Offline.LocalProfile.0")); UserIndex = 0;
        RestorePlatformProfileHint(UserIndex);
    }
    else
    {
        // A local-player array position is not a console storage identity. Never touch a user-0
        // compartment before the actual owning local platform account has been resolved.
        AccountNamespace.Reset(); UserIndex = INDEX_NONE; bPlatformStorageOwnerAvailable = false;
    }
    InitialSaveHandle = UNarrativeSaveSubsystem::OnInitialSaveRequested.AddUObject(this, &USovSaveSubsystem::ResolveInitialSave);
    if (GEngine) { TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &USovSaveSubsystem::OnTravelFailure); }
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USovSaveSubsystem::Tick));
}
void USovSaveSubsystem::Deinitialize()
{
    bEnding = true; ++StorageGeneration;
    if (GEngine) { GEngine->OnTravelFailure().Remove(TravelFailureHandle); }
    UNarrativeSaveSubsystem::OnInitialSaveRequested.Remove(InitialSaveHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    AcknowledgeSaveFailure();
    PendingSave = nullptr; PendingNarrative = nullptr; Storage.Reset();
    Super::Deinitialize();
}
FSovStorageOwnerToken USovSaveSubsystem::CaptureStorageOwner() const
{ return { AccountNamespace, UserIndex, StorageGeneration, !bEnding && !bPlatformSuspended && IsPlatformStorageOwnerAvailable() }; }
bool USovSaveSubsystem::IsStorageOwnerCurrent(const FSovStorageOwnerToken& Token) const
{
    return Token.bAuthorized && !bEnding && !bPlatformSuspended && IsPlatformStorageOwnerAvailable()
        && Token.Generation == StorageGeneration && Token.Namespace == AccountNamespace && Token.User == UserIndex;
}
bool USovSaveSubsystem::AbandonSessionForTitle(FString& Error)
{
    if (bEnding || bBusy) { Error = TEXT("Wait for the current storage callback before returning to title."); return false; }
    ++StorageGeneration;
    bSessionAbandoned = true;
    bPlatformStorageOwnerAvailable = false; // No outgoing-world callback may save after abandonment.
    if (auto* World = GetWorld())
    { if (auto* Narrative = World->GetSubsystem<UNarrativeSaveSubsystem>()) { Narrative->SetSavingDisabled(true); } }
    PendingSave = nullptr; PendingNarrative = nullptr; PendingDestination.Reset(); PendingLoadOwner = {};
    PendingLoadRequest.Invalidate(); PendingLoadDeadline = 0; PendingLoadError.Reset();
    bPendingWorldApplied = false; bPendingLoadFailed = false;
    FailedWrite = nullptr; AcknowledgedWorld.Reset(); AcknowledgmentExpiresAt = 0;
    PendingAutosaves.Reset(); CancelMissionTravel();
    AcknowledgeSaveFailure(); // FailedWrite was retired, so this cannot mint a boundary acknowledgement.
    PublishSettingsOwner();
    if (auto* GI = GetGameInstance())
    { if (auto* Platform = GI->GetSubsystem<USovPlatformServicesSubsystem>()) { Platform->CancelCloudOperation(); } }
    Error.Reset(); return true;
}
bool USovSaveSubsystem::BeginMissionTravel(USovCampaignDefinition* Destination, FString& Error)
{
    const auto Owner = CaptureStorageOwner();
    if (!Destination || !IsStorageOwnerCurrent(Owner) || bBusy || PendingSave || MissionTravelDestination || bAwaitingFailureDecision)
    { Error = TEXT("Mission travel cannot acquire the campaign storage transaction."); return false; }
    MissionTravelDestination = Destination; MissionTravelOwner = Owner;
    MissionTravelDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    MissionTravelFailure.Reset(); bMissionTravelRecoveryAttempted = false;
    Error.Reset(); return true;
}
void USovSaveSubsystem::CancelMissionTravel()
{
    MissionTravelDestination = nullptr; MissionTravelOwner = {}; MissionTravelDeadline = 0;
    MissionTravelFailure.Reset(); bMissionTravelRecoveryAttempted = false;
}
void USovSaveSubsystem::CompleteMissionTravel(USovCampaignDefinition* Destination)
{
    if (!Destination || Destination != MissionTravelDestination || !MissionTravelFailure.IsEmpty() || bPlatformSuspended) { return; }
    const auto* PC = Controller();
    const auto* World = GetWorld();
    const auto* Mode = World ? World->GetAuthGameMode<ASovCampaignGameMode>() : nullptr;
    const auto* Pawn = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
    const bool SameOwner = IsStorageOwnerCurrent(MissionTravelOwner);
    if (!SameOwner || !Mode || Mode->InitialMission != Destination || !PC || PC->GetWorld() != World
        || !Pawn || !Pawn->IsCharacterReady() || PC->GetCampaignState()->GetActiveMission() != Destination
        || FPackageName::GetShortName(Destination->Map.ToSoftObjectPath().GetLongPackageName()) != UGameplayStatics::GetCurrentLevelName(World, true))
    { FailMissionTravel(TEXT("Mission readiness did not retain its original owner and destination.")); return; }
    CancelMissionTravel();
}
void USovSaveSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type, const FString& Error)
{
    if (bEnding || !World || World->GetGameInstance() != GetGameInstance()) { return; }
    if (bSessionAbandoned)
    {
        if (auto* PC = Controller()) { PC->NotifyTitleTravelFailed(Error.IsEmpty() ? TEXT("Title travel failed; the abandoned campaign remains fenced. Retry returning to title.") : Error); }
        return;
    }
    if (MissionTravelDestination) { FailMissionTravel(Error.IsEmpty() ? TEXT("Mission map travel failed.") : Error); }
    else if (PendingSave)
    { bPendingLoadFailed = true; PendingLoadError = Error.IsEmpty() ? TEXT("Saved-map travel failed.") : Error; }
}
void USovSaveSubsystem::FailMissionTravel(const FString& Error)
{
    // Initialization delegates must unwind before recovery can initiate another map load.
    if (MissionTravelDestination && MissionTravelFailure.IsEmpty()) { MissionTravelFailure = Error; }
}
void USovSaveSubsystem::PublishSettingsOwner()
{
    // Unregistered test instances must never write the editor user's real account preferences.
    if (!GetGameInstance() || GetGameInstance()->GetSubsystem<USovSaveSubsystem>() != this) { return; }
    const FString Namespace = AccountNamespace; const int32 User = UserIndex;
    const bool Authorized = !bEnding && IsPlatformStorageOwnerAvailable();
    if (auto* Settings = USovGameUserSettings::Get())
    { Settings->ObserveVerifiedSettingsOwner(Namespace, User, Authorized); }
}
bool USovSaveSubsystem::SelectPlatformUser(const FString& Id, int32 LocalUserIndex, FString& Error)
{
    if (bEnding || bSessionAbandoned || bPlatformSuspended || bBusy || PendingSave || bAwaitingFailureDecision || MissionTravelDestination)
    { Error = TEXT("Finish or cancel the save/load transaction before changing platform user."); return false; }
    if (Id.TrimStartAndEnd().IsEmpty() || LocalUserIndex < 0)
    { Error = TEXT("A stable platform account and nonnegative local user index are required."); return false; }
    if (bRequiresNativePlatformAuthorization && (!bHasNativePlatformAuthorization
        || Id != AuthorizedPlatformId || LocalUserIndex != AuthorizedPlatformLocalUser))
    { Error = TEXT("This platform account has not been authorized by the native account provider."); return false; }
    const FString NewNamespace = FMD5::HashAnsiString(*Id);
    const uint64 Expected = StorageGeneration;
    if (AccountNamespace != NewNamespace || UserIndex != LocalUserIndex)
    {
        // Never stamp the outgoing user's live campaign records as another user's save.
        if (!AccountNamespace.IsEmpty() && !CanManagePlatformSaves(Error)) { return false; }
        TGuardValue<bool> Selection(bBusy, true);
        if (!PersistPlatformProfileHint(NewNamespace, LocalUserIndex, Error)) { return false; }
        if (bEnding || bPlatformSuspended || StorageGeneration != Expected || (bRequiresNativePlatformAuthorization
            && (!bHasNativePlatformAuthorization || Id != AuthorizedPlatformId || LocalUserIndex != AuthorizedPlatformLocalUser)))
        { Error = TEXT("Platform ownership changed while selecting the profile. Selection was not published."); return false; }
        PendingAutosaves.Reset(); PlaySeconds = 0;
        AcknowledgedWorld.Reset(); AcknowledgmentExpiresAt = 0;
    }
    if (AccountNamespace != NewNamespace || UserIndex != LocalUserIndex || !bPlatformStorageOwnerAvailable) { ++StorageGeneration; }
    AccountNamespace = NewNamespace; UserIndex = LocalUserIndex;
    bPlatformStorageOwnerAvailable = true;
    const auto Selected = CaptureStorageOwner(); PublishSettingsOwner();
    if (!IsStorageOwnerCurrent(Selected)) { Error = TEXT("Platform owner was revoked while loading profile preferences."); return false; }
    Error.Reset(); return true;
}
bool USovSaveSubsystem::RestorePlatformProfileHint(int32 LocalUserIndex)
{
    if (!Storage || LocalUserIndex < 0) { return false; }
    FProfileHint Best;
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; FProfileHint Hint;
        if (Storage->Read(ProfileHintSlot(LocalUserIndex, Bank), LocalUserIndex, Bytes)
            && DecodeProfileHint(Bytes, LocalUserIndex, Hint) && Hint.Generation > Best.Generation) { Best = Hint; }
    }
    if (Best.Generation <= 0) { return false; }
    AccountNamespace = Best.Namespace; UserIndex = LocalUserIndex; return true;
}
bool USovSaveSubsystem::PersistPlatformProfileHint(const FString& Namespace, int32 LocalUserIndex, FString& Error)
{
    if (bEnding || bPlatformSuspended || !Storage || LocalUserIndex < 0 || !IsOpaqueNamespace(Namespace))
    { Error = TEXT("Platform profile storage is unavailable."); return false; }
    FProfileHint Best; int32 BestBank = -1;
    const uint64 Expected = StorageGeneration;
    const auto Current = [&]() { return !bEnding && !bPlatformSuspended && Storage && Expected == StorageGeneration; };
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; FProfileHint Hint;
        if (!Current()) { Error = TEXT("Profile ownership changed during hint read."); return false; }
        if (Storage->Read(ProfileHintSlot(LocalUserIndex, Bank), LocalUserIndex, Bytes) && Current()
            && DecodeProfileHint(Bytes, LocalUserIndex, Hint) && Hint.Generation > Best.Generation) { Best = Hint; BestBank = Bank; }
    }
    if (!Current()) { Error = TEXT("Profile ownership changed during hint read."); return false; }
    if (Best.Namespace == Namespace) { return true; }
    if (Best.Generation == MAX_int64) { Error = TEXT("Platform profile generation limit reached."); return false; }
    const TArray<uint8> Bytes = EncodeProfileHint(Namespace, LocalUserIndex, Best.Generation + 1);
    const FString Slot = ProfileHintSlot(LocalUserIndex, BestBank == 0 ? 1 : 0);
    TArray<uint8> Readback; FProfileHint Verified;
    if (!Current() || !Storage->Write(Slot, LocalUserIndex, Bytes) || !Current()
        || !Storage->Read(Slot, LocalUserIndex, Readback) || !Current() || Readback != Bytes
        || !DecodeProfileHint(Readback, LocalUserIndex, Verified) || Verified.Namespace != Namespace)
    { Error = TEXT("Could not retain the selected profile for offline restart. Previous profile and campaign remain selected."); return false; }
    return true;
}
void USovSaveSubsystem::ObserveNativePlatformAccount(const FSovObservedPlatformAccount& Account)
{
    if (bEnding) { return; }
    const bool PreviousAuthorized = bHasNativePlatformAuthorization, PreviousAvailable = bPlatformStorageOwnerAvailable;
    const FString PreviousId = AuthorizedPlatformId;
    const int32 PreviousUser = AuthorizedPlatformLocalUser;
    bRequiresNativePlatformAuthorization = !PLATFORM_DESKTOP || Account.bRequiresKnownStorageOwner;
    bHasNativePlatformAuthorization = Account.bIdentityKnown && !Account.StableId.IsEmpty() && Account.LocalUser >= 0
        && (!bRequiresNativePlatformAuthorization || Account.bStorageAccessAuthorized);
    AuthorizedPlatformId = bHasNativePlatformAuthorization ? Account.StableId : FString();
    AuthorizedPlatformLocalUser = bHasNativePlatformAuthorization ? Account.LocalUser : INDEX_NONE;
    if (!bHasNativePlatformAuthorization)
    {
        // Unknown desktop identity may be an outage. Unknown console ownership never grants access.
        if (bRequiresNativePlatformAuthorization) { bPlatformStorageOwnerAvailable = false; }
    }
    else
    {
        const FString OfflineId = FString::Printf(TEXT("Offline.LocalProfile.%d"), UserIndex);
        bPlatformStorageOwnerAvailable = UserIndex >= 0 && UserIndex == Account.LocalUser
            && (AccountNamespace == FMD5::HashAnsiString(*Account.StableId)
                || (!bRequiresNativePlatformAuthorization && AccountNamespace == FMD5::HashAnsiString(*OfflineId)));
    }
    if (bSessionAbandoned)
    {
        FString FrontendError;
        const auto* World = GetWorld();
        if (World && World->GetAuthGameMode() && !Cast<ASovCampaignGameMode>(World->GetAuthGameMode())
            && CanManagePlatformSaves(FrontendError)) { bSessionAbandoned = false; ++StorageGeneration; }
        else { bPlatformStorageOwnerAvailable = false; }
    }
    if (PreviousAuthorized != bHasNativePlatformAuthorization || PreviousAvailable != bPlatformStorageOwnerAvailable
        || PreviousId != AuthorizedPlatformId || PreviousUser != AuthorizedPlatformLocalUser)
    { ++StorageGeneration; PublishSettingsOwner(); }
}
void USovSaveSubsystem::FenceUnresolvedAccountChange()
{
    ++StorageGeneration; bPlatformStorageOwnerAvailable = false; bHasNativePlatformAuthorization = false;
    AuthorizedPlatformId.Reset(); AuthorizedPlatformLocalUser = INDEX_NONE; PublishSettingsOwner();
}
void USovSaveSubsystem::SetPlatformSuspended(bool bSuspended)
{
    if (bPlatformSuspended == bSuspended) { return; }
    const double Now = FPlatformTime::Seconds();
    const uint64 BeforeSuspensionChange = StorageGeneration;
    ++StorageGeneration;
    // Persistent restore/travel receipts survive only this suspension transition. A native owner
    // revocation (including revoke/re-authorize ABA) has already changed the generation and cannot
    // be repaired by resume. Ordinary I/O receipts are never forwarded across suspension.
    auto ForwardPersistentOwner = [&](FSovStorageOwnerToken& Owner)
    {
        if (Owner.bAuthorized && Owner.Generation == BeforeSuspensionChange && IsPlatformStorageOwnerAvailable()
            && Owner.Namespace == AccountNamespace && Owner.User == UserIndex) { Owner.Generation = StorageGeneration; }
    };
    ForwardPersistentOwner(MissionTravelOwner); ForwardPersistentOwner(PendingLoadOwner);
    bPlatformSuspended = bSuspended;
    if (bSuspended) { PlatformSuspendedAt = Now; }
    else
    {
        const double Elapsed = FMath::Max(0.0, Now - PlatformSuspendedAt);
        if (PendingLoadDeadline > 0) { PendingLoadDeadline += Elapsed; }
        if (MissionTravelDeadline > 0) { MissionTravelDeadline += Elapsed; }
        if (AcknowledgmentExpiresAt > 0) { AcknowledgmentExpiresAt += Elapsed; }
        bDiscardPlatformResumeDelta = true; PlatformSuspendedAt = 0;
    }
    // Publish only after the complete local transition, so settings callbacks cannot be overwritten.
    if (GetGameInstance() && GetGameInstance()->GetSubsystem<USovSaveSubsystem>() == this)
    { if (auto* Settings = USovGameUserSettings::Get()) { Settings->SetPlatformSettingsSuspended(bSuspended); } }
}
bool USovSaveSubsystem::CanManagePlatformSaves(FString& Error) const
{
    if (bEnding || bPlatformSuspended || bBusy || PendingSave || bAwaitingFailureDecision || MissionTravelDestination)
    { Error = TEXT("Finish the current save/load or save-failure decision first."); return false; }
    const ASovPlayerController* PC = Controller();
    const UWorld* World = GetWorld();
    // The mode check closes the interval before the destination controller is ready.
    if ((PC && ((PC->GetCampaignState() && PC->GetCampaignState()->GetActiveMission())
            || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle))
        || (World && Cast<ASovCampaignGameMode>(World->GetAuthGameMode())))
    { Error = TEXT("Return to the front end before changing account or importing a cloud save."); return false; }
    Error.Reset(); return true;
}
bool USovSaveSubsystem::ExportPlatformSnapshot(ESovSaveSlotKind Kind, int32 Index, TArray<uint8>& Bytes,
    FSovSaveSlotHeader& Header, bool& bExists, FString& Error)
{
    Bytes.Reset(); Header = {}; bExists = false;
    if (bPlatformSuspended || !Storage || !bPlatformStorageOwnerAvailable || AccountNamespace.IsEmpty() || !SovSavePolicy::ValidSlot(PolicyKind(Kind), Index))
    { Error = TEXT("Save storage, account or slot is unavailable."); return false; }
    if (bBusy || PendingSave || bAwaitingFailureDecision)
    { Error = TEXT("Finish the current local save transaction first."); return false; }
    const auto Owner = CaptureStorageOwner();
    int32 Bank; bool Damaged;
    TStrongObjectPtr<USovCampaignSaveGame> Save(ReadBest(Kind, Index, Bank, Damaged, Error));
    if (!IsStorageOwnerCurrent(Owner)) { Error = TEXT("Save owner changed during export."); return false; }
    // Do not turn a damaged local slot into an apparently empty cloud-import target.
    if (Damaged) { Error = TEXT("Recover the damaged local save before comparing cloud copies."); return false; }
    if (!Save.IsValid()) { Error.Reset(); return true; }
    if (!Storage->Read(BankName(Kind, Index, Bank), Owner.User, Bytes) || !IsStorageOwnerCurrent(Owner)
        || !ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error) || !IsStorageOwnerCurrent(Owner))
    { Bytes.Reset(); return false; }
    bExists = true; Error.Reset(); return true;
}
bool USovSaveSubsystem::ValidatePlatformSnapshot(const TArray<uint8>& Bytes, ESovSaveSlotKind Kind, int32 Index,
    FSovSaveSlotHeader& Header, FString& Error) const
{
    // Size is checked before Unreal deserializes provider-controlled bytes.
    if (Bytes.IsEmpty() || Bytes.Num() > 64 * 1024 * 1024)
    { Error = TEXT("Cloud save is empty or exceeds the supported 64 MiB envelope limit."); return false; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(USovCampaignSaveGame::DecodeFramed(Bytes, Error));
    if (!ValidateEnvelope(Save.Get(), false, Error)) { return false; }
    if (Save->Header.Kind != Kind || Save->Header.SlotIndex != Index || Save->Header.Generation <= 0)
    { Error = TEXT("Cloud save belongs to a different slot or has an invalid generation."); return false; }
    // Review is not a world load. Import performs the full asset/canon preflight again before mutation.
    Header = Save->Header; return true;
}
ESovSaveResult USovSaveSubsystem::ImportPlatformSnapshot(const TArray<uint8>& Bytes,
    const TArray<uint8>& ReviewedLocalBytes, ESovSaveSlotKind Kind, int32 Index, FString& Error)
{
    if (!CanManagePlatformSaves(Error)) { return ESovSaveResult::UnsafeState; }
    const auto Owner = CaptureStorageOwner();
    FSovSaveSlotHeader Header;
    if (!ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error)) { return ESovSaveResult::IncompatibleSave; }
    TStrongObjectPtr<USovCampaignSaveGame> Candidate(USovCampaignSaveGame::DecodeFramed(Bytes, Error));
    if (!ValidateEnvelope(Candidate.Get(), true, Error) || !DecodeNarrative(Candidate.Get(), Error))
    { return ESovSaveResult::IncompatibleSave; }
    // Synchronous required-asset loads can pump events. Recheck admission/account after preflight.
    if (!IsStorageOwnerCurrent(Owner) || !CanManagePlatformSaves(Error) || !ValidateEnvelope(Candidate.Get(), false, Error))
    { Error = TEXT("Save import owner changed during validation."); return ESovSaveResult::UnsafeState; }
    return CommitPlatformSnapshot(Bytes, ReviewedLocalBytes, Kind, Index, Error);
}
ESovSaveResult USovSaveSubsystem::CommitPlatformSnapshot(const TArray<uint8>& Bytes,
    const TArray<uint8>& ReviewedLocalBytes, ESovSaveSlotKind Kind, int32 Index, FString& Error)
{
    const auto Owner = CaptureStorageOwner();
    FSovSaveSlotHeader Header; bool Exists; TArray<uint8> Current;
    if (!ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error)) { return ESovSaveResult::IncompatibleSave; }
    if (!ExportPlatformSnapshot(Kind, Index, Current, Header, Exists, Error)) { return ESovSaveResult::CorruptSave; }
    if (!IsStorageOwnerCurrent(Owner)) { Error = TEXT("Save import owner changed."); return ESovSaveResult::MissingAccount; }
    if (Current != ReviewedLocalBytes)
    { Error = TEXT("Local save changed after review. Compare the copies again before importing."); return ESovSaveResult::Busy; }
    TGuardValue<bool> Mutation(bBusy, true);
    // Two explicitly reviewed import generations retain at most four archives per logical slot.
    // Existing pre-policy GUID archives are untouched; no user file is silently removed by migration.
    const FString ArchivePrefix = BankName(Kind, Index, 0) + TEXT("_CloudReview_") + FString::FromInt(static_cast<int32>(Header.Generation & 1));
    auto Preserve = [&](const FString& Suffix, const TArray<uint8>& Data)
    {
        if (Data.IsEmpty()) { return true; }
        TArray<uint8> Readback;
        return IsStorageOwnerCurrent(Owner) && Storage->Write(ArchivePrefix + Suffix, Owner.User, Data)
            && IsStorageOwnerCurrent(Owner) && Storage->Read(ArchivePrefix + Suffix, Owner.User, Readback)
            && IsStorageOwnerCurrent(Owner) && Readback == Data;
    };
    if (!Preserve(TEXT("_Local"), Current) || !Preserve(TEXT("_Remote"), Bytes))
    { Error = TEXT("Could not durably preserve both reviewed copies. Local save banks were not changed."); return ESovSaveResult::WriteFailed; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(USovCampaignSaveGame::DecodeFramed(Bytes, Error));
    if (!IsStorageOwnerCurrent(Owner)) { Error = TEXT("Save owner changed during import."); return ESovSaveResult::MissingAccount; }
    // The native writer assigns the next LOCAL generation and preserves the previous good bank.
    // Cloud revision clocks/generations are never used to select or rename local bank authority.
    return WriteEnvelope(Save.Get(), Error);
}
ASovPlayerController* USovSaveSubsystem::Controller() const
{
    return GetGameInstance() ? Cast<ASovPlayerController>(GetGameInstance()->GetFirstLocalPlayerController()) : nullptr;
}
FString USovSaveSubsystem::BankName(ESovSaveSlotKind Kind, int32 Index, int32 Bank) const
{
    return FString::Printf(TEXT("SovCampaign_v1_%s_%d_%d_%c"), *AccountNamespace, static_cast<int32>(Kind), Index, Bank ? 'B' : 'A');
}
bool USovSaveSubsystem::CanCapture(FString& Error) const
{ return CanCaptureInternal(Error, false); }
bool USovSaveSubsystem::CanCaptureInternal(FString& Error, bool bAllowEntrySuspension) const
{
    if (bEnding || MissionTravelDestination) { Error = TEXT("The campaign session is ending or travelling."); return false; }
    if (bPlatformSuspended) { Error = TEXT("Save capture is held while the application is suspended."); return false; }
    if (!bPlatformStorageOwnerAvailable)
    { Error = TEXT("The campaign's platform storage owner is unavailable. Restore the original account or return to the front end to choose a profile."); return false; }
    const ASovPlayerController* PC = Controller();
    const ASovPlayerCharacterBase* Pawn = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
    const ASovPlayerState* PS = PC ? PC->GetPlayerState<ASovPlayerState>() : nullptr;
    const USovCampaignStateComponent* State = PC ? PC->GetCampaignState() : nullptr;
    const UAbilitySystemComponent* ASC = PS ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ASovPlayerState*>(PS)) : nullptr;
    const USovTechniqueComponent* Techniques = PS ? Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent()) : nullptr;
    const UWorld* World = PC ? PC->GetWorld() : nullptr;
    const UNarrativeSaveSubsystem* Narrative = World ? World->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
    if (!PC || !Pawn || !PS || !State || !ASC || !World || !Narrative || !Techniques)
    { Error = TEXT("A fully initialized campaign player is required to save."); return false; }
    SovSavePolicy::Admission Admission;
    Admission.Authority = PC->HasAuthority(); Admission.Standalone = World->GetNetMode() == NM_Standalone;
    Admission.Ready = Pawn->IsCharacterReady(); Admission.Alive = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f;
    Admission.StableIdentity = ASC->GetAvatarActor() == Pawn && State->GetActiveMission()
        && State->GetActiveProtagonist() == Pawn->GetProtagonistIdentityTag();
    Admission.StateValid = State->IsStateValid() && Techniques->IsTechniqueStateValid();
    Admission.Grounded = Pawn->GetCharacterMovement() && Pawn->GetCharacterMovement()->IsMovingOnGround();
    Admission.Mutating = PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || Techniques->IsTechniqueMutationInProgress() || State->IsMutationInProgress() || Narrative->IsLoading();
    Admission.SavingDisabled = Narrative->IsSavingDisabled();
    const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
    const ASovEncounterDirector* QuiescentEntry = nullptr;
    if (bAllowEntrySuspension)
    {
        for (TActorIterator<ASovEncounterDirector> It(const_cast<UWorld*>(World)); It; ++It)
        {
            if (It->IsEntryCheckpointQuiescentForSave(Pawn))
            { if (QuiescentEntry) { Error = TEXT("Multiple entry checkpoints claim this player."); return false; } QuiescentEntry = *It; }
        }
    }
    Admission.InCinematic = ASC->HasMatchingGameplayTag(N.State_SequencerControlled);
    Admission.UnresolvedChoice = ASC->HasMatchingGameplayTag(N.State_DialogueControlled) || ASC->HasMatchingGameplayTag(N.State_Interacting)
        || ASC->HasMatchingGameplayTag(S.State_Choice_Unresolved);
    FGameplayTagContainer Blocked;
    Blocked.AddTag(N.State_Busy); Blocked.AddTag(N.State_Movement_Lock); Blocked.AddTag(N.State_Movement_Climbing);
    Blocked.AddTag(N.State_Movement_Ragdoll); Blocked.AddTag(N.State_Weapon_Equipping);
    Blocked.AddTag(S.State_EchoAbility_Active); Blocked.AddTag(S.State_Guarding); Blocked.AddTag(S.State_Deflecting);
    Blocked.AddTag(S.State_Fatal); Blocked.AddTag(S.State_Poise_Broken); Blocked.AddTag(S.State_Guard_Broken);
    Blocked.AddTag(S.State_Traversal);
    Admission.InTraversal = ASC->HasAnyMatchingGameplayTags(Blocked);
    for (TActorIterator<ASovEncounterDirector> It(const_cast<UWorld*>(World)); It; ++It)
    {
        const auto Encounter = It->GetEncounterState();
        if (Encounter == ESovEncounterState::Active || Encounter == ESovEncounterState::Restoring)
        { Admission.InCombat = true; break; }
    }
    const auto* Team = Cast<INarrativeTeamAgentInterface>(Pawn);
    if (!Team) { Admission.NearbyThreat = true; }
    for (TActorIterator<APawn> It(const_cast<UWorld*>(World)); Team && It; ++It)
    {
        if (*It == Pawn || Team->GetTeamAttitudeTowards(**It) != ETeamAttitude::Hostile) { continue; }
        if (QuiescentEntry && !QuiescentEntry->FindParticipantId(*It).IsNone()) { continue; }
        const auto* OtherASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(*It);
        if (OtherASC && (OtherASC->HasMatchingGameplayTag(N.State_IsDead)
            || OtherASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f)) { continue; }
        const auto* AI = Cast<AAIController>(It->GetController());
        if (FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) <= FMath::Square(1500.f)
            || (AI && AI->GetFocusActor() == Pawn)) { Admission.NearbyThreat = true; break; }
    }
    if (!SovSavePolicy::CanCapture(Admission))
    { Error = TEXT("Save unavailable during combat, cinematics, traversal, dialogue/choices, transitions or incomplete state."); return false; }
    return true;
}

bool USovSaveSubsystem::ValidateEnvelope(USovCampaignSaveGame* Save, bool bValidateAssets, FString& Error) const
{
    if (!Save || !Save->HasValidIntegrity()) { Error = TEXT("Save integrity check failed; the original bank is preserved."); return false; }
    const auto& H = Save->Header;
    if (Save->RequiredAssets.Num() > 4096 || H.MigrationHistory.Num() > 64 || H.Build.Len() > 4096
        || H.AccountNamespace.Len() > 128 || H.MapPackage.Len() > 4096 || Save->NarrativePayload.Num() > 60 * 1024 * 1024)
    { Error = TEXT("Save records exceed the supported campaign budget."); return false; }
    if (SovSavePolicy::CheckVersion(H.SchemaMajor, H.SchemaMinor,
        H.Product == TEXT("SovereignCall.Origins.Campaign"), H.AccountNamespace == AccountNamespace)
        != SovSavePolicy::Compatibility::Compatible)
    { Error = TEXT("Save schema/product/account is incompatible. December 2025 prototype saves are not supported; use a compatible v2 save."); return false; }
    if (!SovSavePolicy::ValidSlot(PolicyKind(H.Kind), H.SlotIndex) || !FMath::IsFinite(H.PlaySeconds) || H.PlaySeconds < 0
        || H.MissionId.IsNone() || !H.ActiveProtagonist.IsValid() || !FPackageName::IsValidLongPackageName(H.MapPackage) || !H.MissionDefinition.IsValid())
    { Error = TEXT("Required save metadata is missing or invalid."); return false; }
    if (!USovGameUserSettings::ValidatePortableSettings(Save->PortableSettings, Error)) { return false; }
    if (bValidateAssets)
    {
        if (!FPackageName::DoesPackageExist(H.MapPackage)) { Error = TEXT("The saved mission map is unavailable."); return false; }
        for (const auto& Asset : Save->RequiredAssets)
        { if (!Asset.IsValid() || !Asset.TryLoad()) { Error = TEXT("A required save asset is unavailable: ") + Asset.ToString(); return false; } }
        auto* Mission = Cast<USovCampaignDefinition>(H.MissionDefinition.TryLoad());
        if (!Mission || Mission->MissionId != H.MissionId || Mission->Map.ToSoftObjectPath().GetLongPackageName() != H.MapPackage
            || !ASovPlayerController::ValidateMissionPawn(Mission, Error, H.ActiveProtagonist))
        { if (Error.IsEmpty()) { Error = TEXT("Saved mission identity or definition is incompatible."); } return false; }
    }
    return true;
}
USovCampaignSaveGame* USovSaveSubsystem::ReadBest(ESovSaveSlotKind Kind, int32 Index, int32& OutBank, bool& bDamaged, FString& Error)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SovSave_ReadVerifiedBanks);
    OutBank = -1; bDamaged = false;
    if (bPlatformSuspended || !Storage || !bPlatformStorageOwnerAvailable || !SovSavePolicy::ValidSlot(PolicyKind(Kind), Index) || AccountNamespace.IsEmpty()) { return nullptr; }
    TStrongObjectPtr<USovCampaignSaveGame> Banks[2];
    const auto Owner = CaptureStorageOwner();
    SovSavePolicy::Bank Valid[2];
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; const FString Name = BankName(Kind, Index, Bank);
        if (!IsStorageOwnerCurrent(Owner)) { Error = TEXT("Save owner changed while reading banks."); return nullptr; }
        const bool Exists = Storage->Exists(Name, Owner.User);
        if (!IsStorageOwnerCurrent(Owner)) { Error = TEXT("Save owner changed while checking banks."); return nullptr; }
        if (!Exists) { continue; }
        if (Storage->Read(Name, Owner.User, Bytes) && IsStorageOwnerCurrent(Owner))
        { Banks[Bank].Reset(USovCampaignSaveGame::DecodeFramed(Bytes, Error)); }
        if (!IsStorageOwnerCurrent(Owner)) { Error = TEXT("Save owner changed during bank decoding."); return nullptr; }
        FString ValidationError;
        if (ValidateEnvelope(Banks[Bank].Get(), false, ValidationError)
            && Banks[Bank]->Header.Kind == Kind && Banks[Bank]->Header.SlotIndex == Index)
        { Valid[Bank] = { true, Banks[Bank]->Header.Generation }; }
        else { bDamaged = true; Error = ValidationError; }
    }
    OutBank = SovSavePolicy::LatestBank(Valid[0], Valid[1]);
    return OutBank >= 0 ? Banks[OutBank].Get() : nullptr;
}
ESovSaveResult USovSaveSubsystem::WriteEnvelope(USovCampaignSaveGame* Save, FString& Error)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SovSave_WriteAndVerify);
    const auto Owner = CaptureStorageOwner();
    const auto Current = [&]()
    { if (IsStorageOwnerCurrent(Owner)) { return true; } Error = TEXT("Save authorization changed; no further storage operation is allowed."); return false; };
    if (bPlatformSuspended) { Error = TEXT("Save writes are held while the application is suspended."); return ESovSaveResult::Busy; }
    if (!bPlatformStorageOwnerAvailable)
    { Error = TEXT("Original platform save owner is unavailable. The existing campaign and save banks were not changed."); return ESovSaveResult::MissingAccount; }
    if (!Storage || !Save) { Error = TEXT("Save storage is unavailable."); return ESovSaveResult::WriteFailed; }
    int32 OldBank = -1; bool Damaged = false;
    TStrongObjectPtr<USovCampaignSaveGame> Previous(ReadBest(Save->Header.Kind, Save->Header.SlotIndex, OldBank, Damaged, Error));
    if (!Current()) { return ESovSaveResult::MissingAccount; }
    int64 Generation = Previous.IsValid() ? Previous->Header.Generation : 0;
    // Generation is global within the rolling autosave group so rotation remains deterministic after restart.
    if (Save->Header.Kind == ESovSaveSlotKind::Auto)
    {
        for (int32 Index = 0; Index < SovSavePolicy::AutoSlots; ++Index)
        { int32 Bank; bool Bad; FString Ignored; auto* Other = ReadBest(ESovSaveSlotKind::Auto, Index, Bank, Bad, Ignored);
          if (!Current()) { return ESovSaveResult::MissingAccount; }
          if (Other) { Generation = FMath::Max(Generation, Other->Header.Generation); } }
    }
    int32 TargetBank = 0; std::int64_t Next = 0;
    const SovSavePolicy::Bank A { OldBank == 0, Generation }, B { OldBank == 1, Generation };
    if (!SovSavePolicy::NextWrite(A, B, TargetBank, Next))
    { Error = TEXT("Save generation limit reached."); return ESovSaveResult::WriteFailed; }
    if (Generation == MAX_int64) { Error = TEXT("Save generation limit reached."); return ESovSaveResult::WriteFailed; }
    if (OldBank < 0) { Next = Generation + 1; }
    if (Save->bReadLegacyUnframed || (Previous.IsValid() && Previous->bReadLegacyUnframed))
    { Save->Header.MigrationHistory.AddUnique(TEXT("Envelope.RawUnframedToSVF2.v1")); }
    Save->Header.Generation = Next; Save->IntegrityChecksum = Save->CalculateChecksum();
    if (!ValidateEnvelope(Save, false, Error)) { return ESovSaveResult::CaptureFailed; }
    TArray<uint8> Bytes;
    if (!USovCampaignSaveGame::EncodeFramed(Save, Bytes, Error))
    { Error = TEXT("Campaign envelope serialization failed; previous save retained."); return ESovSaveResult::CaptureFailed; }
    const FString TargetName = BankName(Save->Header.Kind, Save->Header.SlotIndex, TargetBank);
    if (!Current()) { return ESovSaveResult::MissingAccount; }
    // Preserve a corrupt bank byte-for-byte for support before reusing its logical position.
    const bool TargetExists = Storage->Exists(TargetName, Owner.User);
    if (!Current()) { return ESovSaveResult::MissingAccount; }
    if (TargetExists)
    {
        TArray<uint8> ExistingBytes;
        if (!Storage->Read(TargetName, Owner.User, ExistingBytes) || !Current())
        { Error = TEXT("Existing save bank cannot be read safely; it was not overwritten."); return ESovSaveResult::WriteFailed; }
        FString ExistingError;
        TStrongObjectPtr<USovCampaignSaveGame> Existing(USovCampaignSaveGame::DecodeFramed(ExistingBytes, ExistingError));
        if (!Current()) { return ESovSaveResult::MissingAccount; }
        if (!ValidateEnvelope(Existing.Get(), false, ExistingError)
            || Existing->Header.Kind != Save->Header.Kind || Existing->Header.SlotIndex != Save->Header.SlotIndex)
        {
            FString RecoveryName;
            for (int32 Archive = 0; Archive < 4; ++Archive)
            {
                if (!Current()) { return ESovSaveResult::MissingAccount; }
                const FString Candidate = TargetName + TEXT("_Recovery_") + FString::FromInt(Archive);
                const bool Occupied = Storage->Exists(Candidate, Owner.User);
                if (!Current()) { return ESovSaveResult::MissingAccount; }
                if (!Occupied) { RecoveryName = Candidate; break; }
            }
            if (RecoveryName.IsEmpty())
            { Error = TEXT("Preserved corrupt-bank archive capacity is full. Return to title and explicitly remove a reviewed archive; current source banks are retained."); return ESovSaveResult::WriteFailed; }
            TArray<uint8> RecoveryBytes;
            if (!Current() || !Storage->Write(RecoveryName, Owner.User, ExistingBytes) || !Current()
                || !Storage->Read(RecoveryName, Owner.User, RecoveryBytes) || !Current()
                || RecoveryBytes != ExistingBytes)
            { Error = TEXT("Corrupt save could not be preserved for support; it was not overwritten."); return ESovSaveResult::WriteFailed; }
        }
    }
    if (!Current()) { return ESovSaveResult::MissingAccount; }
    if (!Storage->Write(TargetName, Owner.User, Bytes))
    { Error = TEXT("Platform save write failed. Previous good bank retained; free storage or continue without saving explicitly."); return ESovSaveResult::WriteFailed; }
    TArray<uint8> Readback;
    if (!Current()) { return ESovSaveResult::MissingAccount; }
    if (!Storage->Read(TargetName, Owner.User, Readback) || !Current() || Readback != Bytes)
    { Error = TEXT("Save readback failed. Previous good bank retained; the new save is not confirmed."); return ESovSaveResult::ReadbackFailed; }
    TStrongObjectPtr<USovCampaignSaveGame> Verified(USovCampaignSaveGame::DecodeFramed(Readback, Error));
    if (!ValidateEnvelope(Verified.Get(), false, Error) || !Current()) { return ESovSaveResult::ReadbackFailed; }
    Error.Reset(); return ESovSaveResult::Success;
}
ESovSaveResult USovSaveSubsystem::CaptureAndWrite(ESovSaveSlotKind Kind, int32 Index, FName BoundaryId, FString& Error, bool bAllowEntrySuspension, ESovSaveBoundary Boundary)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SovSave_CaptureCampaign);
    if (bBusy || PendingSave) { return ESovSaveResult::Busy; }
    if (bAwaitingFailureDecision) { return ESovSaveResult::AwaitingFailureDecision; }
    if (!SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return ESovSaveResult::InvalidSlot; }
    if (AccountNamespace.IsEmpty() || !bPlatformStorageOwnerAvailable)
    { Error = TEXT("The selected campaign's platform storage owner is unavailable."); return ESovSaveResult::MissingAccount; }
    if (!CanCaptureInternal(Error, bAllowEntrySuspension)) { return ESovSaveResult::UnsafeState; }
    const auto Owner = CaptureStorageOwner();
    TGuardValue<bool> Mutation(bBusy, true);
    ASovPlayerController* PC = Controller(); APawn* Pawn = PC->GetPawn();
    auto* Mission = PC->GetCampaignState()->GetActiveMission();
    UNarrativeSave* Snapshot = nullptr;
    if (!PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>()->CaptureSaveObject(Snapshot))
    { Error = TEXT("Narrative actor/component serialization failed; previous save retained."); return ESovSaveResult::CaptureFailed; }
    TStrongObjectPtr<UNarrativeSave> KeepSnapshot(Snapshot);
    if (!IsStorageOwnerCurrent(Owner) || Controller() != PC || PC->GetPawn() != Pawn || PC->GetCampaignState()->GetActiveMission() != Mission || !CanCaptureInternal(Error, bAllowEntrySuspension))
    { Error = TEXT("Campaign ownership or safe state changed during serialization."); return ESovSaveResult::CaptureFailed; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(NewObject<USovCampaignSaveGame>(this));
    auto& H = Save->Header;
    H.Kind = Kind; H.SlotIndex = Index; H.AccountNamespace = AccountNamespace;
    H.TimestampUtc = FDateTime::UtcNow(); H.PlaySeconds = PlaySeconds;
    H.Build = FString(FApp::GetBuildVersion()) + TEXT("|") + FEngineVersion::Current().ToString(); H.MissionId = Mission->MissionId; H.MissionLabel = Mission->DisplayName;
    H.ActiveProtagonist = PC->GetCampaignState()->GetActiveProtagonist();
    H.MissionDefinition = FSoftObjectPath(Mission); H.MapPackage = Mission->Map.ToSoftObjectPath().GetLongPackageName(); H.BoundaryId = BoundaryId; H.BoundaryKind = Boundary;
    Save->RequiredAssets.AddUnique(H.MissionDefinition);
    Save->RequiredAssets.AddUnique(Mission->ResolvePawnClass(H.ActiveProtagonist).ToSoftObjectPath());
    Save->RequiredAssets.AddUnique(Mission->ResolvePlayerDefinition(H.ActiveProtagonist).ToSoftObjectPath());
    const auto CollectRequiredAssets = [&Save](const FNarrativeActorRecord& Record)
    {
        if (!Record.bDestroyed && !Record.bOptional) { Save->RequiredAssets.AddUnique(Record.ActorSoftClass.ToSoftObjectPath()); }
        for (const auto& Component : Record.SavedComponents)
        { if (!Record.bOptional && !Component.bOptional && !Component.ComponentClass.IsNull()) { Save->RequiredAssets.AddUnique(Component.ComponentClass.ToSoftObjectPath()); } }
    };
    for (const auto& Pair : Snapshot->RecordMap) { CollectRequiredAssets(Pair.Value); }
    CollectRequiredAssets(Snapshot->PlayerData.ControllerData); CollectRequiredAssets(Snapshot->PlayerData.PlayerStateData);
    CollectRequiredAssets(Snapshot->PlayerData.PawnData);
    if (auto* Settings = USovGameUserSettings::Get())
    { H.Difficulty = Settings->GetDifficultyId(); if (!Settings->CapturePortableSettings(Save->PortableSettings)) { return ESovSaveResult::CaptureFailed; } }
    else { Error = TEXT("Campaign settings are unavailable."); return ESovSaveResult::CaptureFailed; }
    if (!UGameplayStatics::SaveGameToMemory(Snapshot, Save->NarrativePayload))
    { Error = TEXT("Narrative save subclass serialization failed."); return ESovSaveResult::CaptureFailed; }
    if (!IsStorageOwnerCurrent(Owner) || Controller() != PC || PC->GetPawn() != Pawn || !CanCaptureInternal(Error, bAllowEntrySuspension))
    { Error = TEXT("Campaign capture was superseded during configured save serialization."); return ESovSaveResult::CaptureFailed; }
    const ESovSaveResult Result = WriteEnvelope(Save.Get(), Error);
    if (Result == ESovSaveResult::WriteFailed || Result == ESovSaveResult::ReadbackFailed) { FailedWrite = Save.Get(); }
    ReportSave(Result, H, Error);
    return Result;
}
ESovSaveResult USovSaveSubsystem::SaveManual(int32 Index, FString& Error)
{ return CaptureAndWrite(ESovSaveSlotKind::Manual, Index, NAME_None, Error); }
ESovSaveResult USovSaveSubsystem::WriteCheckpoint(ESovSaveBoundary Boundary, FName BoundaryId, FString& Error)
{
    if (BoundaryId.IsNone()) { Error = TEXT("A stable checkpoint boundary ID is required."); return ESovSaveResult::InvalidSlot; }
    const auto Result = CaptureAndWrite(ESovSaveSlotKind::Checkpoint, 0, BoundaryId, Error,
        Boundary == ESovSaveBoundary::ArenaEntry || Boundary == ESovSaveBoundary::BossRetry, Boundary);
    if (Result == ESovSaveResult::Success) { QueueAutosave(Boundary, BoundaryId); }
    return Result;
}
void USovSaveSubsystem::QueueAutosave(ESovSaveBoundary Boundary, FName Id)
{
    if (Id.IsNone()) { return; }
    for (const auto& Pending : PendingAutosaves) { if (Pending.Kind == Boundary && Pending.Id == Id) { return; } }
    // Safe boundaries in the same frame coalesce to the latest coherent state.
    PendingAutosaves.Reset(); PendingAutosaves.Add({ Boundary, Id });
}
TArray<FSovSaveSlotHeader> USovSaveSubsystem::ListSlots()
{
    TArray<FSovSaveSlotHeader> Result; const auto Owner = CaptureStorageOwner();
    if (!IsStorageOwnerCurrent(Owner)) { return Result; }
    for (int32 Kind = 0; Kind < 3; ++Kind)
    {
        const auto Type = static_cast<ESovSaveSlotKind>(Kind);
        const int32 Count = Type == ESovSaveSlotKind::Manual ? SovSavePolicy::ManualSlots : Type == ESovSaveSlotKind::Auto ? SovSavePolicy::AutoSlots : 1;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            int32 Bank; bool Bad; FString Error;
            auto* Save = ReadBest(Type, Index, Bank, Bad, Error);
            if (!IsStorageOwnerCurrent(Owner)) { return {}; }
            if (Save) { Result.Add(Save->Header); }
        }
    }
    return Result;
}
TArray<FString> USovSaveSubsystem::ListPreservedSaveArchives(ESovSaveSlotKind Kind, int32 Index)
{
    TArray<FString> Archives; FString Error;
    const auto Owner = CaptureStorageOwner();
    if (!CanManagePlatformSaves(Error) || !IsStorageOwnerCurrent(Owner) || !Storage || !SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return Archives; }
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        for (int32 Archive = 0; Archive < 4; ++Archive)
        {
            const FString Id = BankName(Kind, Index, Bank) + TEXT("_Recovery_") + FString::FromInt(Archive);
            if (!IsStorageOwnerCurrent(Owner)) { return {}; }
            const bool Exists = Storage->Exists(Id, Owner.User);
            if (!IsStorageOwnerCurrent(Owner)) { return {}; }
            if (Exists) { Archives.Add(Id); }
        }
    }
    return Archives;
}
bool USovSaveSubsystem::DeletePreservedSaveArchive(ESovSaveSlotKind Kind, int32 Index, const FString& ArchiveId, FString& Error)
{
    const FString Requested = ArchiveId; const auto Owner = CaptureStorageOwner();
    if (!CanManagePlatformSaves(Error) || !IsStorageOwnerCurrent(Owner) || !Storage) { return false; }
    if (!ListPreservedSaveArchives(Kind, Index).Contains(Requested) || !IsStorageOwnerCurrent(Owner))
    { Error = TEXT("Choose an existing preserved archive for this exact owner and slot."); return false; }
    TGuardValue<bool> Mutation(bBusy, true);
    int32 Bank; bool Damaged;
    TStrongObjectPtr<USovCampaignSaveGame> Good(ReadBest(Kind, Index, Bank, Damaged, Error));
    if (!Good.IsValid() || !IsStorageOwnerCurrent(Owner))
    { Error = TEXT("Retain a verified native campaign bank before deleting support archives."); return false; }
    if (!Storage->Remove(Requested, Owner.User) || !IsStorageOwnerCurrent(Owner))
    { Error = TEXT("Archive deletion was not confirmed by platform storage."); return false; }
    const bool StillExists = Storage->Exists(Requested, Owner.User);
    if (!IsStorageOwnerCurrent(Owner) || StillExists) { Error = TEXT("Archive deletion could not be verified."); return false; }
    Error.Reset(); return true;
}
bool USovSaveSubsystem::FindRecoveryAutosave(FSovSaveSlotHeader& Slot)
{
    bool Found = false; const auto Owner = CaptureStorageOwner();
    for (const auto& Header : ListSlots())
    {
        if (Header.Kind == ESovSaveSlotKind::Auto && (!Found || Header.Generation > Slot.Generation))
        {
            int32 Bank; bool Bad; FString Error;
            // Required asset preflight can synchronously load packages and collect garbage.
            TStrongObjectPtr<USovCampaignSaveGame> Save(ReadBest(Header.Kind, Header.SlotIndex, Bank, Bad, Error));
            if (!IsStorageOwnerCurrent(Owner)) { Slot = {}; return false; }
            const bool Valid = ValidateEnvelope(Save.Get(), true, Error) && IsStorageOwnerCurrent(Owner) && DecodeNarrative(Save.Get(), Error);
            if (!IsStorageOwnerCurrent(Owner)) { Slot = {}; return false; }
            if (Valid) { Slot = Header; Found = true; }
        }
    }
    return Found;
}
UNarrativeSave* USovSaveSubsystem::DecodeNarrative(USovCampaignSaveGame* Save, FString& Error) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SovSave_DecodeNarrative);
    UClass* ConfiguredClass = UNarrativeSave::StaticClass();
    if (const auto* Settings = GetDefault<USaveSystemDeveloperSettings>(); Settings && Settings->SaveGameClass.IsValid())
    { ConfiguredClass = Settings->SaveGameClass.TryLoadClass<UNarrativeSave>(); }
    auto* Snapshot = Save ? Cast<UNarrativeSave>(USovCampaignSaveGame::DecodeKnownSave(
        Save->NarrativePayload, ConfiguredClass, GetTransientPackage(), Error)) : nullptr;
    TStrongObjectPtr<UNarrativeSave> KeepSnapshot(Snapshot); // Required/optional synchronous asset loads may collect unreachable objects.
    if (!Snapshot || Snapshot->RecordMap.Num() > 4096 || !Snapshot->PlayerData.IsValid() || !Snapshot->PlayerData.ControllerData.IsValid()
        || !Snapshot->PlayerData.PlayerStateData.IsValid())
    { Error = TEXT("Required Narrative player records are unavailable."); return nullptr; }
    const auto ValidRecordMetadata = [](const FNarrativeActorRecord& Record)
    {
        if (!Record.IsValid() || Record.SavedComponents.Num() > 128 || Record.ByteData.Num() > 16 * 1024 * 1024
            || Record.Transform.ContainsNaN() || Record.ActorSoftClass.IsNull()
            || static_cast<uint8>(Record.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)) { return false; }
        TSet<FName> ComponentNames;
        for (const auto& Component : Record.SavedComponents)
        {
            if (Component.ByteData.Num() > 16 * 1024 * 1024 || Component.ComponentName.IsNone() || ComponentNames.Contains(Component.ComponentName)
                || static_cast<uint8>(Component.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)) { return false; }
            ComponentNames.Add(Component.ComponentName);
        }
        return true;
    };
    if (!ValidRecordMetadata(Snapshot->PlayerData.ControllerData) || !ValidRecordMetadata(Snapshot->PlayerData.PlayerStateData)
        || !ValidRecordMetadata(Snapshot->PlayerData.PawnData))
    { Error = TEXT("Required player record identities or restore phases are incompatible."); return nullptr; }
    bool ValidCampaign = false;
    for (const auto& Record : Snapshot->PlayerData.ControllerData.SavedComponents)
    {
        if (Record.ComponentName == GetDefault<ASovPlayerController>()->GetCampaignState()->GetFName())
        {
            FGameplayTag SavedHero;
            ValidCampaign = USovCampaignStateComponent::GetSerializedActiveProtagonist(Record.ByteData, SavedHero, Error)
                && SavedHero == Save->Header.ActiveProtagonist;
            break;
        }
    }
    if (!ValidCampaign) { if (Error.IsEmpty()) { Error = TEXT("Required canon-state record is missing or incompatible."); } return nullptr; }
    for (auto It = Snapshot->RecordMap.CreateIterator(); It; ++It)
    {
        auto& Record = It.Value();
        if (Record.bOptional && !Record.bDestroyed && !Record.ActorSoftClass.LoadSynchronous())
        { UE_LOG(LogTemp, Warning, TEXT("Skipping unavailable optional save actor %s"), *Record.ActorName.ToString()); It.RemoveCurrent(); continue; }
        for (int32 Index = Record.SavedComponents.Num() - 1; Index >= 0; --Index)
        {
            const auto& Component = Record.SavedComponents[Index];
            if (Component.bOptional && !Component.ComponentClass.IsNull() && !Component.ComponentClass.LoadSynchronous())
            { UE_LOG(LogTemp, Warning, TEXT("Skipping unavailable optional save component %s"), *Component.ComponentName.ToString()); Record.SavedComponents.RemoveAt(Index); }
        }
        if (!It.Key().IsValid() || Record.ActorGUID != It.Key() || !ValidRecordMetadata(Record))
        { Error = TEXT("A checkpoint actor has an invalid stable identity."); return nullptr; }
    }
    return Snapshot;
}
ESovSaveResult USovSaveSubsystem::LoadSlot(ESovSaveSlotKind Kind, int32 Index, FString& Error, bool bAcceptRecoveredBank)
{
    if (bPlatformSuspended) { Error = TEXT("Save loading is held while the application is suspended."); return ESovSaveResult::Busy; }
    // Even a rejected initialization retains its request until the terminal event
    // has been published. A retry must not overwrite that pending completion.
    if (bBusy || PendingSave) { return ESovSaveResult::Busy; }
    if (!SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return ESovSaveResult::InvalidSlot; }
    if (AccountNamespace.IsEmpty() || !bPlatformStorageOwnerAvailable)
    { Error = TEXT("The selected campaign's platform storage owner is unavailable."); return ESovSaveResult::MissingAccount; }
    ASovPlayerController* PC = Controller();
    if (!PC || !PC->HasAuthority() || PC->GetWorld()->GetNetMode() != NM_Standalone)
    { Error = TEXT("Campaign load requires the standalone local controller."); return ESovSaveResult::UnsafeState; }
    TGuardValue<bool> Mutation(bBusy, true);
    const auto Owner = CaptureStorageOwner();
    int32 Bank; bool Damaged;
    PendingSave = ReadBest(Kind, Index, Bank, Damaged, Error);
    if (!IsStorageOwnerCurrent(Owner)) { PendingSave = nullptr; Error = TEXT("Save owner changed while opening the slot."); return ESovSaveResult::MissingAccount; }
    if (!PendingSave) { return Damaged ? ESovSaveResult::CorruptSave : ESovSaveResult::MissingSave; }
    if (Damaged && !bAcceptRecoveredBank)
    {
        const auto Header = PendingSave->Header; PendingSave = nullptr;
        Error = TEXT("A damaged bank was detected. A verified previous state is available; confirm recovery using its displayed mission and timestamp.");
        OnLoadCompleted.Broadcast(ESovSaveResult::RecoveryAvailable, Header, Error);
        return ESovSaveResult::RecoveryAvailable;
    }
    if (!ValidateEnvelope(PendingSave, true, Error)) { PendingSave = nullptr; return ESovSaveResult::IncompatibleSave; }
    PendingNarrative = DecodeNarrative(PendingSave, Error);
    if (!PendingNarrative) { PendingSave = nullptr; return ESovSaveResult::IncompatibleSave; }
    if (!IsStorageOwnerCurrent(Owner) || Controller() != PC)
    { PendingSave = nullptr; PendingNarrative = nullptr; Error = TEXT("Save owner changed during load preflight."); return ESovSaveResult::MissingAccount; }
    PendingLoadOwner = Owner;
    bPendingWorldApplied = false; bPendingLoadFailed = false; PendingDestination.Reset();
    PendingLoadError.Reset(); PendingLoadRequest = FGuid::NewGuid();
    PendingLoadDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    const FString Destination = PendingSave->Header.MapPackage + TEXT("?SovCampaignSlotLoad=1?SovCampaignLoadRequest=")
        + PendingLoadRequest.ToString(EGuidFormats::Digits);
    AcknowledgeSaveFailure();
    if (!IsStorageOwnerCurrent(Owner) || Controller() != PC)
    { PendingSave = nullptr; PendingNarrative = nullptr; PendingLoadRequest.Invalidate(); PendingLoadDeadline = 0;
      Error = TEXT("Save owner changed before load travel."); return ESovSaveResult::MissingAccount; }
    if (!PC->GetWorld()->ServerTravel(Destination, true))
    {
        PendingSave = nullptr; PendingNarrative = nullptr; PendingLoadRequest.Invalidate(); PendingLoadDeadline = 0;
        Error = TEXT("Saved-map travel was rejected; current world retained."); return ESovSaveResult::TravelFailed;
    }
    // Acceptance starts asynchronous map/managed-pawn restoration. Success notification occurs only at CharacterReady.
    return ESovSaveResult::LoadStarted;
}
void USovSaveSubsystem::ResolveInitialSave(UWorld& World, UNarrativeSave*& Snapshot, bool& bOverride)
{
    if (World.GetGameInstance() != GetGameInstance()) { return; }
    const AGameModeBase* GM = World.GetAuthGameMode();
    if (!GM || !UGameplayStatics::HasOption(GM->OptionsString, TEXT("SovCampaignSlotLoad"))) { return; }
    // A timed-out or superseded slot travel must never fall through to a new campaign.
    // Reject stale callbacks without consuming or failing a newer request.
    bOverride = true; Snapshot = nullptr;
    if (!MatchesPendingLoadRequest(GM->OptionsString)) { return; }
    PendingDestination = &World;
    FString Error;
    if (bPendingWorldApplied || !ValidatePendingWorld(World, Error))
    {
        bPendingLoadFailed = true;
        PendingLoadError = Error.IsEmpty() ? TEXT("The saved world attempted to apply its snapshot more than once.") : Error;
        return; // The core ticker publishes failure after world initialization unwinds.
    }
    Snapshot = PendingNarrative;
    bPendingWorldApplied = true;
}
bool USovSaveSubsystem::MatchesPendingLoadRequest(const FString& Options) const
{
    FGuid Request;
    return PendingSave && PendingLoadRequest.IsValid()
        && UGameplayStatics::HasOption(Options, TEXT("SovCampaignSlotLoad"))
        && FGuid::ParseExact(UGameplayStatics::ParseOption(Options, TEXT("SovCampaignLoadRequest")), EGuidFormats::Digits, Request)
        && Request == PendingLoadRequest;
}
bool USovSaveSubsystem::ValidatePendingWorld(UWorld& World, FString& Error) const
{
    const AGameModeBase* Mode = World.GetAuthGameMode();
    if (Mode && UGameplayStatics::HasOption(Mode->OptionsString, TEXT("SovCampaignSlotLoad"))
        && !MatchesPendingLoadRequest(Mode->OptionsString))
    { Error = TEXT("This saved-map request has expired or was replaced. Select a valid recovery save."); return false; }
    if (bPendingLoadFailed && RejectedLoadWorld.Get() == &World)
    { Error = TEXT("This campaign world failed restoration; choose a valid recovery save."); return false; }
    if (!PendingSave) { return true; }
    if (PendingLoadOwner.bAuthorized && (!IsPlatformStorageOwnerAvailable() || PendingLoadOwner.Generation != StorageGeneration
        || PendingLoadOwner.Namespace != AccountNamespace || PendingLoadOwner.User != UserIndex))
    { Error = TEXT("Original save owner is unavailable during restoration."); return false; }
    const auto* GM = Cast<ASovCampaignGameMode>(World.GetAuthGameMode());
    const auto* Narrative = World.GetSubsystem<UNarrativeSaveSubsystem>();
    if (bPendingLoadFailed || !GM || !GM->InitialMission || GM->InitialMission->MissionId != PendingSave->Header.MissionId
        || FSoftObjectPath(GM->InitialMission) != PendingSave->Header.MissionDefinition
        || (Narrative && Narrative->DidInitialLoadFail()))
    { Error = TEXT("Saved destination initialization failed. Choose a last known-good autosave; the source banks remain intact."); return false; }
    return true;
}
void USovSaveSubsystem::NotifyCampaignReady(ASovPlayerController* PC, bool bSucceeded)
{
    if (MissionTravelDestination && !bSucceeded && PC && PC->GetGameInstance() == GetGameInstance())
    { FailMissionTravel(TEXT("Destination campaign initialization failed.")); return; }
    if (!PendingSave || !PC || PC->GetWorld() != PendingDestination.Get()
        || !PC->GetWorld()->GetAuthGameMode()
        || !MatchesPendingLoadRequest(PC->GetWorld()->GetAuthGameMode()->OptionsString)) { return; }
    FString Error;
    const bool Good = bSucceeded && bPendingWorldApplied && ValidatePendingWorld(*PC->GetWorld(), Error)
        && PC->GetPawn() && Cast<ASovPlayerCharacterBase>(PC->GetPawn())
        && CastChecked<ASovPlayerCharacterBase>(PC->GetPawn())->IsCharacterReady();
    if (!Good && Error.IsEmpty()) { Error = TEXT("Campaign restoration failed; choose a compatible autosave."); }
    CompletePendingLoad(Good, Error);
}
void USovSaveSubsystem::CompletePendingLoad(bool bSucceeded, const FString& Error)
{
    if (!PendingSave) { return; }
    const auto Header = PendingSave->Header;
    if (bSucceeded) { PlaySeconds = Header.PlaySeconds; RejectedLoadWorld.Reset(); }
    else { RejectedLoadWorld = PendingDestination; }
    PendingAutosaves.Reset();
    PendingSave = nullptr; PendingNarrative = nullptr; PendingDestination.Reset();
    PendingLoadRequest.Invalidate(); PendingLoadDeadline = 0; PendingLoadError.Reset();
    PendingLoadOwner = {};
    bPendingWorldApplied = false; bPendingLoadFailed = !bSucceeded;
    // All ownership is released before observers may request a different recovery slot.
    OnLoadCompleted.Broadcast(bSucceeded ? ESovSaveResult::Success : ESovSaveResult::RecoveryAvailable, Header, Error);
}
void USovSaveSubsystem::ReportSave(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Error)
{
    if (Result == ESovSaveResult::WriteFailed || Result == ESovSaveResult::ReadbackFailed)
    {
        bAwaitingFailureDecision = true; PausedController = Controller();
        if (PausedController.IsValid())
        { bOwnPause = PausedController->AcquireSystemPause(TEXT("SaveFailure")); }
    }
    OnSaveCompleted.Broadcast(Result, Header, Error);
}
ESovSaveResult USovSaveSubsystem::RetryFailedWrite(FString& Error)
{
    if (bBusy || PendingSave) { return ESovSaveResult::Busy; }
    if (!bAwaitingFailureDecision || !FailedWrite) { Error = TEXT("No failed write is waiting for retry."); return ESovSaveResult::MissingSave; }
    TGuardValue<bool> Mutation(bBusy, true);
    TStrongObjectPtr<USovCampaignSaveGame> Retained(FailedWrite);
    const auto Result = WriteEnvelope(Retained.Get(), Error);
    const auto Header = Retained->Header;
    if (Result == ESovSaveResult::Success) { FailedWrite = nullptr; AcknowledgeSaveFailure(); }
    ReportSave(Result, Header, Error);
    return Result;
}
bool USovSaveSubsystem::ConsumeAcknowledgedBoundary(ESovSaveBoundary Boundary, FName Id)
{
    const ASovPlayerController* PC = Controller();
    if (bPlatformSuspended || !PC || PendingSave || !AcknowledgedWorld.IsValid() || PC->GetWorld() != AcknowledgedWorld.Get()
        || FPlatformTime::Seconds() > AcknowledgmentExpiresAt || AcknowledgedBoundary.BoundaryKind != Boundary
        || Id.IsNone() || AcknowledgedBoundary.BoundaryId != Id || !PC->GetCampaignState()->GetActiveMission()
        || PC->GetCampaignState()->GetActiveMission()->MissionId != AcknowledgedBoundary.MissionId
        || PC->GetCampaignState()->GetActiveProtagonist() != AcknowledgedBoundary.ActiveProtagonist) { return false; }
    AcknowledgedWorld.Reset(); AcknowledgmentExpiresAt = 0;
    return true;
}
void USovSaveSubsystem::AcknowledgeSaveFailure()
{
    if (bAwaitingFailureDecision && FailedWrite && FailedWrite->Header.Kind == ESovSaveSlotKind::Checkpoint)
    {
        AcknowledgedBoundary = FailedWrite->Header;
        AcknowledgedWorld = Controller() ? Controller()->GetWorld() : nullptr;
        AcknowledgmentExpiresAt = (bPlatformSuspended ? PlatformSuspendedAt : FPlatformTime::Seconds()) + 60.0;
    }
    if (bOwnPause && PausedController.IsValid()) { PausedController->ReleaseSystemPause(TEXT("SaveFailure")); }
    bOwnPause = false; bAwaitingFailureDecision = false; PausedController.Reset();
    FailedWrite = nullptr; PendingAutosaves.Reset();
}
bool USovSaveSubsystem::Tick(float DeltaSeconds)
{
    if (bEnding || bPlatformSuspended || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f || bBusy) { return true; }
    // Core ticker may report the entire suspended interval on its first foreground frame.
    if (bDiscardPlatformResumeDelta) { bDiscardPlatformResumeDelta = false; return true; }
    ASovPlayerController* PC = Controller();
    if (MissionTravelDestination && MissionTravelFailure.IsEmpty() && PC && PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle
        && PC->GetCampaignState()->GetActiveMission() == MissionTravelDestination)
    { CompleteMissionTravel(MissionTravelDestination); } // Complete a readiness notification deferred during suspension.
    if (MissionTravelDestination && (!MissionTravelFailure.IsEmpty() || FPlatformTime::Seconds() > MissionTravelDeadline))
    {
        const auto Owner = MissionTravelOwner;
        const FString Failure = MissionTravelFailure.IsEmpty() ? TEXT("Mission travel initialization timed out.") : MissionTravelFailure;
        const bool AlreadyRetried = bMissionTravelRecoveryAttempted;
        CancelMissionTravel(); bMissionTravelRecoveryAttempted = true;
        const uint64 Expected = StorageGeneration;
        if (PC) { PC->NotifyMissionTravelFailed(Failure); }
        if (bEnding || StorageGeneration != Expected || MissionTravelDestination || PendingSave) { return true; }
        FString RecoveryError;
        ESovSaveResult Result = ESovSaveResult::MissingAccount;
        if (!PC && !AlreadyRetried)
        {
            // An early InitNewPlayer rejection may leave no controller capable of owning LoadSlot.
            // Return once to the configured title, where a fresh controller can offer explicit recovery.
            UWorld* World = GetWorld(); FString TitlePath;
            if (GConfig) { GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), TitlePath, GEngineIni); }
            const FString Package = FPackageName::ObjectPathToPackageName(TitlePath);
            if (World && World->GetNetMode() == NM_Standalone && !Package.IsEmpty() && FPackageName::DoesPackageExist(Package)
                && FPackageName::GetShortName(Package) != UGameplayStatics::GetCurrentLevelName(World, true)
                && AbandonSessionForTitle(RecoveryError) && World->ServerTravel(Package, true)) { return true; }
            RecoveryError = TEXT("No controller remains for checkpoint recovery, and the configured title could not load. Source saves are retained.");
            UE_LOG(LogTemp, Error, TEXT("%s"), *RecoveryError);
        }
        if (!AlreadyRetried && IsStorageOwnerCurrent(Owner))
        { Result = LoadSlot(ESovSaveSlotKind::Checkpoint, 0, RecoveryError); }
        if (Result != ESovSaveResult::LoadStarted && Result != ESovSaveResult::RecoveryAvailable)
        {
            FSovSaveSlotHeader Header;
            OnLoadCompleted.Broadcast(ESovSaveResult::RecoveryAvailable, Header,
                Failure + TEXT(" Restore the original account and choose a checkpoint, or return to title. ") + RecoveryError);
        }
        return true;
    }
    if (PC && PC->GetPawn() && PC->GetCampaignState()->GetActiveMission() && !UGameplayStatics::IsGamePaused(PC))
    { PlaySeconds += DeltaSeconds; }
    if (PendingSave && bPendingWorldApplied && !bPendingLoadFailed && PendingDestination.IsValid())
    {
        const auto* Narrative = PendingDestination->GetSubsystem<UNarrativeSaveSubsystem>();
        if (Narrative && Narrative->DidInitialLoadFail())
        {
            bPendingLoadFailed = true;
            PendingLoadError = TEXT("The saved world's actor or component restoration failed. Select a compatible recovery save.");
        }
    }
    if (PendingSave && (bPendingLoadFailed || FPlatformTime::Seconds() > PendingLoadDeadline))
    {
        const FString Error = PendingLoadError.IsEmpty()
            ? TEXT("Saved-map initialization timed out. Choose a last known-good autosave.") : PendingLoadError;
        CompletePendingLoad(false, Error);
        return true;
    }
    FString Error;
    const bool bEntryBoundary = !PendingAutosaves.IsEmpty() && (PendingAutosaves.Last().Kind == ESovSaveBoundary::ArenaEntry
        || PendingAutosaves.Last().Kind == ESovSaveBoundary::BossRetry);
    if (!PendingSave && !PendingAutosaves.IsEmpty() && !bAwaitingFailureDecision && CanCaptureInternal(Error, bEntryBoundary))
    {
        SovSavePolicy::Bank Slots[SovSavePolicy::AutoSlots];
        for (int32 Index = 0; Index < SovSavePolicy::AutoSlots; ++Index)
        { int32 Bank; bool Bad; auto* Save = ReadBest(ESovSaveSlotKind::Auto, Index, Bank, Bad, Error);
          if (Save) { Slots[Index] = { true, Save->Header.Generation }; } }
        const FQueuedBoundary Boundary = PendingAutosaves.Last();
        const auto Result = CaptureAndWrite(ESovSaveSlotKind::Auto, SovSavePolicy::OldestAuto(Slots), Boundary.Id, Error, bEntryBoundary, Boundary.Kind);
        if (Result == ESovSaveResult::Success) { PendingAutosaves.Reset(); }
    }
    return true;
}
