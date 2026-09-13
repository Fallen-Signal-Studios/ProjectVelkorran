// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovSaveSubsystem.h"
#include "Save/SovSavePolicy.h"
#include "Platform/SovPlatformServicesAdapter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovCampaignGameMode.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Misc/Crc.h"
#include "HAL/PlatformProperties.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "NarrativeGameplayTags.h"
#include "Progression/SovTechniqueComponent.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "UObject/StrongObjectPtr.h"

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
    };
    USovCampaignSaveGame* LoadCampaignEnvelope(const TArray<uint8>& Bytes)
    {
        // Campaign envelopes use the UE5 GVAS header (engine save format 3).
        // The engine treats a missing tag as a pre-GVAS class-name string;
        // damaged leading bytes can then trigger a fatal FName length check.
        // Reject that unsupported legacy path before invoking UObject decoding.
        if (Bytes.Num() < 2 * static_cast<int32>(sizeof(int32))) { return nullptr; }
        FMemoryReader Reader(Bytes, true);
        int32 Magic = 0, Version = 0;
        Reader << Magic << Version;
        if (Reader.IsError() || Magic != 0x53415647 || Version != 3) { return nullptr; }
        return Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    }
    constexpr double LoadTimeoutSeconds = 120.0;
    SovSavePolicy::Kind PolicyKind(ESovSaveSlotKind Kind) { return static_cast<SovSavePolicy::Kind>(Kind); }
    constexpr uint32 ProfileHintMagic = 0x534F5641;
    constexpr int32 ProfileHintBytes = 88;
    struct FProfileHint { FString Namespace; int64 Generation = 0; };
    FString ProfileHintSlot(int32 User, int32 Bank)
    { return FString::Printf(TEXT("SovAccount_v1_%s_%d_%c"), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()), User, Bank ? TCHAR('B') : TCHAR('A')); }
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
            && FString(ANSI_TO_TCHAR(Platform)) == FMD5::HashAnsiString(ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));
    }
    TArray<uint8> EncodeProfileHint(const FString& Namespace, int32 User, int64 Generation)
    {
        TArray<uint8> Bytes; FMemoryWriter Writer(Bytes, true); uint32 Magic = ProfileHintMagic, Version = 1;
        Writer << Magic << Version << User << Generation;
        ANSICHAR Hash[32], Platform[32]; const FString PlatformHash = FMD5::HashAnsiString(ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));
        for (int32 Index = 0; Index < 32; ++Index) { Hash[Index] = static_cast<ANSICHAR>(Namespace[Index]); Platform[Index] = static_cast<ANSICHAR>(PlatformHash[Index]); }
        Writer.Serialize(Hash, 32); Writer.Serialize(Platform, 32);
        uint32 CRC = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num()); Writer << CRC; return Bytes;
    }
}

USovSaveSubsystem::FOperationOwner USovSaveSubsystem::CaptureOperationOwner() const
{
    return { AccountNamespace, UserIndex, SelectionEpoch, AuthorizationEpoch, SuspensionEpoch,
        bPlatformStorageOwnerAvailable };
}
bool USovSaveSubsystem::IsOperationOwnerCurrent(const FOperationOwner& Owner, FString& Error, bool bRequireAvailable) const
{
    if (bShuttingDown || !Storage || bPlatformSuspended || Owner.Namespace != AccountNamespace || Owner.LocalUser != UserIndex
        || Owner.SelectionEpoch != SelectionEpoch || Owner.AuthorizationEpoch != AuthorizationEpoch
        || Owner.SuspensionEpoch != SuspensionEpoch || Owner.bAvailable != bPlatformStorageOwnerAvailable
        || (Owner.bRetryOperation && (!bAwaitingFailureDecision || !Owner.RetrySnapshot.IsValid() || FailedWrite != Owner.RetrySnapshot.Get()))
        || (bRequireAvailable && (!bPlatformStorageOwnerAvailable || AccountNamespace.IsEmpty() || UserIndex < 0)))
    {
        Error = TEXT("Save ownership or application state changed during this operation. No further storage work was issued; any in-flight write is unconfirmed.");
        return false;
    }
    return true;
}
ESovSaveResult USovSaveSubsystem::OwnershipFailureResult() const
{ return bPlatformSuspended ? ESovSaveResult::Busy : ESovSaveResult::MissingAccount; }
bool USovSaveSubsystem::IsPendingLoadOwnerCurrent(FString& Error) const
{ return IsRetainedOwnerCurrent(PendingLoadOwner, Error); }
bool USovSaveSubsystem::IsRetainedOwnerCurrent(const FOperationOwner& Owner, FString& Error) const
{
    // No storage boundary is crossed while applying a previously decoded snapshot. Retain
    // the watchdog's same-owner suspend hold, but never accept account/authorization ABA.
    if (bShuttingDown || !bPlatformStorageOwnerAvailable || Owner.Namespace != AccountNamespace
        || Owner.LocalUser != UserIndex || Owner.SelectionEpoch != SelectionEpoch
        || Owner.AuthorizationEpoch != AuthorizationEpoch)
    {
        Error = TEXT("The account that started this load changed or lost authorization. Select a recovery save with the original owner.");
        return false;
    }
    return true;
}
bool USovSaveSubsystem::ReadOwned(const FOperationOwner& Owner, const FString& Slot, int32 LocalUser,
    TArray<uint8>& Bytes, FString& Error, bool bRequireAvailable)
{
    if (!IsOperationOwnerCurrent(Owner, Error, bRequireAvailable)) { return false; }
    const bool bRead = Storage->Read(Slot, LocalUser, Bytes);
    return IsOperationOwnerCurrent(Owner, Error, bRequireAvailable) && bRead;
}
bool USovSaveSubsystem::WriteOwned(const FOperationOwner& Owner, const FString& Slot, int32 LocalUser,
    const TArray<uint8>& Bytes, FString& Error, bool bRequireAvailable)
{
    if (!IsOperationOwnerCurrent(Owner, Error, bRequireAvailable)) { return false; }
    const bool bWritten = Storage->Write(Slot, LocalUser, Bytes);
    return IsOperationOwnerCurrent(Owner, Error, bRequireAvailable) && bWritten;
}
bool USovSaveSubsystem::ExistsOwned(const FOperationOwner& Owner, const FString& Slot, bool& bExists, FString& Error)
{
    bExists = false;
    if (!IsOperationOwnerCurrent(Owner, Error)) { return false; }
    bExists = Storage->Exists(Slot, Owner.LocalUser);
    return IsOperationOwnerCurrent(Owner, Error);
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
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USovSaveSubsystem::Tick));
    InitializeMissionTravelRecovery();
}
void USovSaveSubsystem::Deinitialize()
{
    bShuttingDown = true; ++SelectionEpoch;
    DeinitializeMissionTravelRecovery();
    UNarrativeSaveSubsystem::OnInitialSaveRequested.Remove(InitialSaveHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    AcknowledgeSaveFailure();
    PendingSave = nullptr; PendingNarrative = nullptr; Storage.Reset();
    Super::Deinitialize();
}
bool USovSaveSubsystem::SelectPlatformUser(const FString& Id, int32 LocalUserIndex, FString& Error)
{
    if (bPlatformSuspended || bBusy || PendingSave || MissionTravelRequest.IsValid() || bAwaitingFailureDecision)
    { Error = TEXT("Finish or cancel the save/load transaction before changing platform user."); return false; }
    if (Id.TrimStartAndEnd().IsEmpty() || LocalUserIndex < 0)
    { Error = TEXT("A stable platform account and nonnegative local user index are required."); return false; }
    if (bRequiresNativePlatformAuthorization && (!bHasNativePlatformAuthorization
        || Id != AuthorizedPlatformId || LocalUserIndex != AuthorizedPlatformLocalUser))
    { Error = TEXT("This platform account has not been authorized by the native account provider."); return false; }
    const FString NewNamespace = FMD5::HashAnsiString(*Id);
    const FOperationOwner Owner = CaptureOperationOwner();
    const bool bChangingProfile = AccountNamespace != NewNamespace || UserIndex != LocalUserIndex;
    if (bChangingProfile)
    {
        // Never stamp the outgoing user's live campaign records as another user's save.
        if (!AccountNamespace.IsEmpty() && !CanManagePlatformSaves(Error)) { return false; }
        TGuardValue<bool> Mutation(bBusy, true);
        if (!PersistPlatformProfileHint(NewNamespace, LocalUserIndex, Error)
            || !IsOperationOwnerCurrent(Owner, Error, false)) { return false; }
        PendingAutosaves.Reset(); PlaySeconds = 0;
        AcknowledgedWorld.Reset(); AcknowledgmentExpiresAt = 0;
    }
    if (!IsOperationOwnerCurrent(Owner, Error, false)) { return false; }
    AccountNamespace = NewNamespace; UserIndex = LocalUserIndex;
    if (bChangingProfile) { ++SelectionEpoch; }
    bPlatformStorageOwnerAvailable = true;
    return true;
}
bool USovSaveSubsystem::RestorePlatformProfileHint(int32 LocalUserIndex)
{
    if (!Storage || LocalUserIndex < 0) { return false; }
    const FOperationOwner Owner = CaptureOperationOwner(); FString Error;
    TGuardValue<bool> Mutation(bBusy, true);
    FProfileHint Best;
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; FProfileHint Hint;
        if (ReadOwned(Owner, ProfileHintSlot(LocalUserIndex, Bank), LocalUserIndex, Bytes, Error, false)
            && DecodeProfileHint(Bytes, LocalUserIndex, Hint) && Hint.Generation > Best.Generation) { Best = Hint; }
        if (!IsOperationOwnerCurrent(Owner, Error, false)) { return false; }
    }
    if (Best.Generation <= 0) { return false; }
    if (AccountNamespace != Best.Namespace || UserIndex != LocalUserIndex) { ++SelectionEpoch; }
    AccountNamespace = Best.Namespace; UserIndex = LocalUserIndex; return true;
}
bool USovSaveSubsystem::PersistPlatformProfileHint(const FString& Namespace, int32 LocalUserIndex, FString& Error)
{
    if (!Storage || LocalUserIndex < 0 || !IsOpaqueNamespace(Namespace))
    { Error = TEXT("Platform profile storage is unavailable."); return false; }
    const FOperationOwner Owner = CaptureOperationOwner();
    TGuardValue<bool> Mutation(bBusy, true);
    FProfileHint Best; int32 BestBank = -1;
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; FProfileHint Hint;
        if (ReadOwned(Owner, ProfileHintSlot(LocalUserIndex, Bank), LocalUserIndex, Bytes, Error, false)
            && DecodeProfileHint(Bytes, LocalUserIndex, Hint) && Hint.Generation > Best.Generation) { Best = Hint; BestBank = Bank; }
        if (!IsOperationOwnerCurrent(Owner, Error, false)) { return false; }
    }
    if (Best.Namespace == Namespace) { return true; }
    if (Best.Generation == MAX_int64) { Error = TEXT("Platform profile generation limit reached."); return false; }
    const TArray<uint8> Bytes = EncodeProfileHint(Namespace, LocalUserIndex, Best.Generation + 1);
    const FString Slot = ProfileHintSlot(LocalUserIndex, BestBank == 0 ? 1 : 0);
    TArray<uint8> Readback; FProfileHint Verified;
    if (!WriteOwned(Owner, Slot, LocalUserIndex, Bytes, Error, false)
        || !ReadOwned(Owner, Slot, LocalUserIndex, Readback, Error, false) || Readback != Bytes
        || !DecodeProfileHint(Readback, LocalUserIndex, Verified) || Verified.Namespace != Namespace)
    { Error = TEXT("Could not retain the selected profile for offline restart. Previous profile and campaign remain selected."); return false; }
    return true;
}
void USovSaveSubsystem::ObserveNativePlatformAccount(const FSovObservedPlatformAccount& Account)
{
    const bool bPreviousRequiresAuthorization = bRequiresNativePlatformAuthorization;
    const bool bPreviousAuthorization = bHasNativePlatformAuthorization;
    const FString PreviousId = AuthorizedPlatformId;
    const int32 PreviousLocalUser = AuthorizedPlatformLocalUser;
    bRequiresNativePlatformAuthorization = !PLATFORM_DESKTOP || Account.bRequiresKnownStorageOwner;
    bHasNativePlatformAuthorization = Account.bIdentityKnown && !Account.StableId.IsEmpty() && Account.LocalUser >= 0
        && (!bRequiresNativePlatformAuthorization || Account.bStorageAccessAuthorized);
    AuthorizedPlatformId = bHasNativePlatformAuthorization ? Account.StableId : FString();
    AuthorizedPlatformLocalUser = bHasNativePlatformAuthorization ? Account.LocalUser : INDEX_NONE;
    if (bPreviousRequiresAuthorization != bRequiresNativePlatformAuthorization || bPreviousAuthorization != bHasNativePlatformAuthorization
        || PreviousId != AuthorizedPlatformId || PreviousLocalUser != AuthorizedPlatformLocalUser) { ++AuthorizationEpoch; }
    if (!bHasNativePlatformAuthorization)
    {
        // Unknown desktop identity may be an outage. Unknown console ownership never grants access.
        if (bRequiresNativePlatformAuthorization) { bPlatformStorageOwnerAvailable = false; }
        return;
    }
    const FString OfflineId = FString::Printf(TEXT("Offline.LocalProfile.%d"), UserIndex);
    bPlatformStorageOwnerAvailable = UserIndex >= 0 && UserIndex == Account.LocalUser
        && (AccountNamespace == FMD5::HashAnsiString(*Account.StableId)
            || (!bRequiresNativePlatformAuthorization && AccountNamespace == FMD5::HashAnsiString(*OfflineId)));
}
void USovSaveSubsystem::SetPlatformSuspended(bool bSuspended)
{
    if (bPlatformSuspended == bSuspended) { return; }
    const double Now = FPlatformTime::Seconds();
    bPlatformSuspended = bSuspended;
    ++SuspensionEpoch;
    if (bSuspended) { PlatformSuspendedAt = Now; return; }
    const double Elapsed = FMath::Max(0.0, Now - PlatformSuspendedAt);
    if (PendingLoadDeadline > 0) { PendingLoadDeadline += Elapsed; }
    if (MissionTravelDeadline > 0) { MissionTravelDeadline += Elapsed; }
    if (AcknowledgmentExpiresAt > 0) { AcknowledgmentExpiresAt += Elapsed; }
    bDiscardPlatformResumeDelta = true;
    PlatformSuspendedAt = 0;
}
bool USovSaveSubsystem::CanManagePlatformSaves(FString& Error) const
{
    if (bPlatformSuspended || bBusy || PendingSave || MissionTravelRequest.IsValid() || bAwaitingFailureDecision)
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
    if (bBusy || PendingSave || MissionTravelRequest.IsValid() || bAwaitingFailureDecision)
    { Error = TEXT("Finish the current local save transaction first."); return false; }
    const FOperationOwner Owner = CaptureOperationOwner();
    TGuardValue<bool> Mutation(bBusy, true);
    int32 Bank; bool Damaged;
    TStrongObjectPtr<USovCampaignSaveGame> Save(ReadBest(Kind, Index, Bank, Damaged, Error, &Owner));
    if (!IsOperationOwnerCurrent(Owner, Error)) { return false; }
    // Do not turn a damaged local slot into an apparently empty cloud-import target.
    if (Damaged) { Error = TEXT("Recover the damaged local save before comparing cloud copies."); return false; }
    if (!Save.IsValid()) { Error.Reset(); return true; }
    if (!ReadOwned(Owner, BankName(Kind, Index, Bank), Owner.LocalUser, Bytes, Error)
        || !ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error))
    { Bytes.Reset(); return false; }
    if (!IsOperationOwnerCurrent(Owner, Error)) { Bytes.Reset(); Header = {}; return false; }
    bExists = true; Error.Reset(); return true;
}
bool USovSaveSubsystem::ValidatePlatformSnapshot(const TArray<uint8>& Bytes, ESovSaveSlotKind Kind, int32 Index,
    FSovSaveSlotHeader& Header, FString& Error) const
{
    // Size is checked before Unreal deserializes provider-controlled bytes.
    if (Bytes.IsEmpty() || Bytes.Num() > 64 * 1024 * 1024)
    { Error = TEXT("Cloud save is empty or exceeds the supported 64 MiB envelope limit."); return false; }
    const FOperationOwner Owner = CaptureOperationOwner();
    if (!IsOperationOwnerCurrent(Owner, Error)) { return false; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(LoadCampaignEnvelope(Bytes));
    if (!IsOperationOwnerCurrent(Owner, Error)) { return false; }
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
    const FOperationOwner Owner = CaptureOperationOwner();
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    FSovSaveSlotHeader Header;
    if (!ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error)) { return ESovSaveResult::IncompatibleSave; }
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    TStrongObjectPtr<USovCampaignSaveGame> Candidate(LoadCampaignEnvelope(Bytes));
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    if (!ValidateEnvelope(Candidate.Get(), true, Error, &Owner) || !DecodeNarrative(Candidate.Get(), Error, &Owner))
    { return ESovSaveResult::IncompatibleSave; }
    // Synchronous required-asset loads can pump events. Recheck admission/account after preflight.
    if (!IsOperationOwnerCurrent(Owner, Error) || !CanManagePlatformSaves(Error)
        || !ValidateEnvelope(Candidate.Get(), false, Error)) { return ESovSaveResult::UnsafeState; }
    return CommitPlatformSnapshot(Bytes, ReviewedLocalBytes, Kind, Index, Error);
}
ESovSaveResult USovSaveSubsystem::CommitPlatformSnapshot(const TArray<uint8>& Bytes,
    const TArray<uint8>& ReviewedLocalBytes, ESovSaveSlotKind Kind, int32 Index, FString& Error)
{
    const FOperationOwner Owner = CaptureOperationOwner();
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    FSovSaveSlotHeader Header; bool Exists; TArray<uint8> Current;
    if (!ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error)) { return ESovSaveResult::IncompatibleSave; }
    if (!ExportPlatformSnapshot(Kind, Index, Current, Header, Exists, Error)) { return ESovSaveResult::CorruptSave; }
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    if (Current != ReviewedLocalBytes)
    { Error = TEXT("Local save changed after review. Compare the copies again before importing."); return ESovSaveResult::Busy; }
    TGuardValue<bool> Mutation(bBusy, true);
    const FString ArchivePrefix = BankName(Kind, Index, 0) + TEXT("_CloudReview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto Preserve = [&](const FString& Suffix, const TArray<uint8>& Data)
    {
        if (Data.IsEmpty()) { return true; }
        TArray<uint8> Readback;
        return WriteOwned(Owner, ArchivePrefix + Suffix, Owner.LocalUser, Data, Error)
            && ReadOwned(Owner, ArchivePrefix + Suffix, Owner.LocalUser, Readback, Error) && Readback == Data;
    };
    if (!Preserve(TEXT("_Local"), Current) || !Preserve(TEXT("_Remote"), Bytes))
    { Error = TEXT("Could not durably preserve both reviewed copies. Local save banks were not changed."); return ESovSaveResult::WriteFailed; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(LoadCampaignEnvelope(Bytes));
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    // The native writer assigns the next LOCAL generation and preserves the previous good bank.
    // Cloud revision clocks/generations are never used to select or rename local bank authority.
    return WriteEnvelope(Save.Get(), Error, &Owner);
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
    if (MissionTravelRequest.IsValid()) { Error = TEXT("A mission travel or recovery transaction owns this campaign."); return false; }
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
    for (TActorIterator<ASovCampaignEncounterObjective> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (!It->IsActorBeingDestroyed() && It->IsResultPending())
        { Error = TEXT("Encounter victory is awaiting its campaign receipt. A retired result requires the verified entry checkpoint."); return false; }
    }
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
    {
        for (TActorIterator<ASovEncounterDirector> It(const_cast<UWorld*>(World)); It; ++It)
        {
            if ((bAllowEntrySuspension && It->IsEntryCheckpointQuiescentForSave(Pawn))
                || It->IsCompletedPhaseBoundaryQuiescentForSave(Pawn))
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
        if (It->IsPhaseEntryCapturePending())
        { Error = TEXT("The carried encounter roster is awaiting its phase entry capture; retain the verified prior boundary."); return false; }
        if (It->IsCampaignReceiptPending())
        { Error = TEXT("Encounter victory has no committed campaign receipt. Reload the verified entry checkpoint."); return false; }
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

bool USovSaveSubsystem::ValidateEnvelope(USovCampaignSaveGame* Save, bool bValidateAssets, FString& Error, const FOperationOwner* Operation) const
{
    if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return false; }
    if (!Save || !Save->HasValidIntegrity()) { Error = TEXT("Save integrity check failed; the original bank is preserved."); return false; }
    const auto& H = Save->Header;
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
        {
            UObject* Loaded = Asset.IsValid() ? Asset.TryLoad() : nullptr;
            if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return false; }
            if (!Loaded) { Error = TEXT("A required save asset is unavailable: ") + Asset.ToString(); return false; }
        }
        auto* Mission = Cast<USovCampaignDefinition>(H.MissionDefinition.TryLoad());
        if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return false; }
        if (!Mission || Mission->MissionId != H.MissionId || Mission->Map.ToSoftObjectPath().GetLongPackageName() != H.MapPackage
            || !ASovPlayerController::ValidateMissionPawn(Mission, Error, H.ActiveProtagonist))
        { if (Error.IsEmpty()) { Error = TEXT("Saved mission identity or definition is incompatible."); } return false; }
        if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return false; }
    }
    return true;
}
USovCampaignSaveGame* USovSaveSubsystem::ReadBest(ESovSaveSlotKind Kind, int32 Index, int32& OutBank, bool& bDamaged, FString& Error,
    const FOperationOwner* Operation)
{
    OutBank = -1; bDamaged = false;
    const FOperationOwner Owner = Operation ? *Operation : CaptureOperationOwner();
    if (!IsOperationOwnerCurrent(Owner, Error) || !SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return nullptr; }
    TGuardValue<bool> Mutation(bBusy, true);
    TStrongObjectPtr<USovCampaignSaveGame> Banks[2];
    SovSavePolicy::Bank Valid[2];
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; const FString Name = BankName(Kind, Index, Bank);
        bool bExists = false;
        if (!ExistsOwned(Owner, Name, bExists, Error)) { return nullptr; }
        if (!bExists) { continue; }
        if (ReadOwned(Owner, Name, Owner.LocalUser, Bytes, Error))
        { Banks[Bank].Reset(LoadCampaignEnvelope(Bytes)); }
        if (!IsOperationOwnerCurrent(Owner, Error)) { return nullptr; }
        FString ValidationError;
        if (ValidateEnvelope(Banks[Bank].Get(), false, ValidationError)
            && Banks[Bank]->Header.Kind == Kind && Banks[Bank]->Header.SlotIndex == Index)
        { Valid[Bank] = { true, Banks[Bank]->Header.Generation }; }
        else { bDamaged = true; Error = ValidationError; }
    }
    OutBank = SovSavePolicy::LatestBank(Valid[0], Valid[1]);
    return OutBank >= 0 ? Banks[OutBank].Get() : nullptr;
}
ESovSaveResult USovSaveSubsystem::WriteEnvelope(USovCampaignSaveGame* Save, FString& Error, const FOperationOwner* Operation)
{
    if (bPlatformSuspended) { Error = TEXT("Save writes are held while the application is suspended."); return ESovSaveResult::Busy; }
    if (!bPlatformStorageOwnerAvailable)
    { Error = TEXT("Original platform save owner is unavailable. The existing campaign and save banks were not changed."); return ESovSaveResult::MissingAccount; }
    if (!Storage || !Save) { Error = TEXT("Save storage is unavailable."); return ESovSaveResult::WriteFailed; }
    const FOperationOwner Owner = Operation ? *Operation : CaptureOperationOwner();
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    TGuardValue<bool> Mutation(bBusy, true);
    TStrongObjectPtr<USovCampaignSaveGame> KeepSave(Save);
    int32 OldBank = -1; bool Damaged = false;
    TStrongObjectPtr<USovCampaignSaveGame> Previous(ReadBest(Save->Header.Kind, Save->Header.SlotIndex, OldBank, Damaged, Error, &Owner));
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    int64 Generation = Previous.IsValid() ? Previous->Header.Generation : 0;
    // Generation is global within the rolling autosave group so rotation remains deterministic after restart.
    if (Save->Header.Kind == ESovSaveSlotKind::Auto)
    {
        for (int32 Index = 0; Index < SovSavePolicy::AutoSlots; ++Index)
        { int32 Bank; bool Bad; FString Ignored; auto* Other = ReadBest(ESovSaveSlotKind::Auto, Index, Bank, Bad, Ignored, &Owner);
          if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
          if (Other) { Generation = FMath::Max(Generation, Other->Header.Generation); } }
    }
    int32 TargetBank = 0; std::int64_t Next = 0;
    const SovSavePolicy::Bank A { OldBank == 0, Generation }, B { OldBank == 1, Generation };
    if (!SovSavePolicy::NextWrite(A, B, TargetBank, Next))
    { Error = TEXT("Save generation limit reached."); return ESovSaveResult::WriteFailed; }
    if (Generation == MAX_int64) { Error = TEXT("Save generation limit reached."); return ESovSaveResult::WriteFailed; }
    if (OldBank < 0) { Next = Generation + 1; }
    Save->Header.Generation = Next; Save->IntegrityChecksum = Save->CalculateChecksum();
    if (!ValidateEnvelope(Save, false, Error)) { return ESovSaveResult::CaptureFailed; }
    TArray<uint8> Bytes;
    if (!UGameplayStatics::SaveGameToMemory(Save, Bytes))
    { Error = TEXT("Campaign envelope serialization failed; previous save retained."); return ESovSaveResult::CaptureFailed; }
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    const FString TargetName = BankName(Save->Header.Kind, Save->Header.SlotIndex, TargetBank);
    // Preserve a corrupt bank byte-for-byte for support before reusing its logical position.
    bool bTargetExists = false;
    if (!ExistsOwned(Owner, TargetName, bTargetExists, Error)) { return OwnershipFailureResult(); }
    if (bTargetExists)
    {
        TArray<uint8> ExistingBytes;
        if (!ReadOwned(Owner, TargetName, Owner.LocalUser, ExistingBytes, Error))
        {
            if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
            Error = TEXT("Existing save bank cannot be read safely; it was not overwritten."); return ESovSaveResult::WriteFailed;
        }
        TStrongObjectPtr<USovCampaignSaveGame> Existing(LoadCampaignEnvelope(ExistingBytes));
        if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
        FString ExistingError;
        if (!ValidateEnvelope(Existing.Get(), false, ExistingError)
            || Existing->Header.Kind != Save->Header.Kind || Existing->Header.SlotIndex != Save->Header.SlotIndex)
        {
            const FString RecoveryName = TargetName + TEXT("_Recovery_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
            TArray<uint8> RecoveryBytes;
            if (!WriteOwned(Owner, RecoveryName, Owner.LocalUser, ExistingBytes, Error)
                || !ReadOwned(Owner, RecoveryName, Owner.LocalUser, RecoveryBytes, Error)
                || RecoveryBytes != ExistingBytes)
            {
                if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
                Error = TEXT("Corrupt save could not be preserved for support; it was not overwritten."); return ESovSaveResult::WriteFailed;
            }
        }
    }
    if (!WriteOwned(Owner, TargetName, Owner.LocalUser, Bytes, Error))
    {
        if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
        Error = TEXT("Platform save write failed. Previous good bank retained; free storage or continue without saving explicitly."); return ESovSaveResult::WriteFailed;
    }
    TArray<uint8> Readback;
    if (!ReadOwned(Owner, TargetName, Owner.LocalUser, Readback, Error) || Readback != Bytes)
    {
        if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
        Error = TEXT("Save readback failed. Previous good bank retained; the new save is not confirmed."); return ESovSaveResult::ReadbackFailed;
    }
    TStrongObjectPtr<USovCampaignSaveGame> Verified(LoadCampaignEnvelope(Readback));
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    if (!ValidateEnvelope(Verified.Get(), false, Error)) { return ESovSaveResult::ReadbackFailed; }
    Error.Reset(); return ESovSaveResult::Success;
}
ESovSaveResult USovSaveSubsystem::CaptureAndWrite(ESovSaveSlotKind Kind, int32 Index, FName BoundaryId, FString& Error, bool bAllowEntrySuspension, ESovSaveBoundary Boundary)
{
    if (bBusy || PendingSave || MissionTravelRequest.IsValid()) { return ESovSaveResult::Busy; }
    if (bAwaitingFailureDecision) { return ESovSaveResult::AwaitingFailureDecision; }
    if (!SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return ESovSaveResult::InvalidSlot; }
    if (AccountNamespace.IsEmpty() || !bPlatformStorageOwnerAvailable)
    { Error = TEXT("The selected campaign's platform storage owner is unavailable."); return ESovSaveResult::MissingAccount; }
    const FOperationOwner Owner = CaptureOperationOwner();
    if (!CanCaptureInternal(Error, bAllowEntrySuspension)) { return ESovSaveResult::UnsafeState; }
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    TGuardValue<bool> Mutation(bBusy, true);
    ASovPlayerController* PC = Controller(); APawn* Pawn = PC->GetPawn();
    auto* Mission = PC->GetCampaignState()->GetActiveMission();
    UNarrativeSave* Snapshot = nullptr;
    if (!PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>()->CaptureSaveObject(Snapshot))
    { Error = TEXT("Narrative actor/component serialization failed; previous save retained."); return ESovSaveResult::CaptureFailed; }
    TStrongObjectPtr<UNarrativeSave> KeepSnapshot(Snapshot);
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    if (Controller() != PC || PC->GetPawn() != Pawn || PC->GetCampaignState()->GetActiveMission() != Mission || !CanCaptureInternal(Error, bAllowEntrySuspension))
    { Error = TEXT("Campaign ownership or safe state changed during serialization."); return ESovSaveResult::CaptureFailed; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(NewObject<USovCampaignSaveGame>(this));
    auto& H = Save->Header;
    H.Kind = Kind; H.SlotIndex = Index; H.AccountNamespace = AccountNamespace;
    H.TimestampUtc = FDateTime::UtcNow(); H.PlaySeconds = PlaySeconds;
    H.Build = FString(FApp::GetBuildVersion()) + TEXT("|") + FEngineVersion::Current().ToString(); H.MissionId = Mission->MissionId; H.MissionLabel = Mission->DisplayName;
    H.ActiveProtagonist = PC->GetCampaignState()->GetActiveProtagonist();
    H.MigrationHistory = PC->GetCampaignState()->GetMigrationHistory();
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
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    const ESovSaveResult Result = WriteEnvelope(Save.Get(), Error, &Owner);
    if (Result == ESovSaveResult::WriteFailed || Result == ESovSaveResult::ReadbackFailed)
    { FailedWrite = Save.Get(); FailedWriteOwner = Owner; }
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
    FString Error; const FOperationOwner Owner = CaptureOperationOwner();
    if (Id.IsNone() || !IsOperationOwnerCurrent(Owner, Error)) { return; }
    for (const auto& Pending : PendingAutosaves) { if (Pending.Kind == Boundary && Pending.Id == Id) { return; } }
    // Safe boundaries in the same frame coalesce to the latest coherent state.
    PendingAutosaves.Reset(); PendingAutosaves.Add({ Boundary, Id, Owner });
}
TArray<FSovSaveSlotHeader> USovSaveSubsystem::ListSlots()
{
    TArray<FSovSaveSlotHeader> Result;
    if (bBusy) { return Result; }
    const FOperationOwner Owner = CaptureOperationOwner();
    TGuardValue<bool> Mutation(bBusy, true);
    for (int32 Kind = 0; Kind < 3; ++Kind)
    {
        const auto Type = static_cast<ESovSaveSlotKind>(Kind);
        const int32 Count = Type == ESovSaveSlotKind::Manual ? SovSavePolicy::ManualSlots : Type == ESovSaveSlotKind::Auto ? SovSavePolicy::AutoSlots : 1;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            int32 Bank; bool Bad; FString Error;
            if (auto* Save = ReadBest(Type, Index, Bank, Bad, Error, &Owner)) { Result.Add(Save->Header); }
            if (!IsOperationOwnerCurrent(Owner, Error)) { Result.Reset(); return Result; }
        }
    }
    return Result;
}
bool USovSaveSubsystem::FindRecoveryAutosave(FSovSaveSlotHeader& Slot)
{
    const FOperationOwner Owner = CaptureOperationOwner();
    bool Found = false;
    for (const auto& Header : ListSlots())
    {
        if (Header.Kind == ESovSaveSlotKind::Auto && (!Found || Header.Generation > Slot.Generation))
        {
            int32 Bank; bool Bad; FString Error;
            // Required asset preflight can synchronously load packages and collect garbage.
            TStrongObjectPtr<USovCampaignSaveGame> Save(ReadBest(Header.Kind, Header.SlotIndex, Bank, Bad, Error, &Owner));
            if (ValidateEnvelope(Save.Get(), true, Error, &Owner) && DecodeNarrative(Save.Get(), Error, &Owner)) { Slot = Header; Found = true; }
            if (!IsOperationOwnerCurrent(Owner, Error)) { Slot = {}; return false; }
        }
    }
    return Found;
}
UNarrativeSave* USovSaveSubsystem::DecodeNarrative(USovCampaignSaveGame* Save, FString& Error, const FOperationOwner* Operation) const
{
    if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return nullptr; }
    auto* Snapshot = Save ? Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Save->NarrativePayload)) : nullptr;
    TStrongObjectPtr<UNarrativeSave> KeepSnapshot(Snapshot); // Required/optional synchronous asset loads may collect unreachable objects.
    if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return nullptr; }
    if (!Snapshot || !Snapshot->PlayerData.IsValid() || !Snapshot->PlayerData.ControllerData.IsValid()
        || !Snapshot->PlayerData.PlayerStateData.IsValid())
    { Error = TEXT("Required Narrative player records are unavailable."); return nullptr; }
    const auto ValidRecordMetadata = [](const FNarrativeActorRecord& Record)
    {
        if (!Record.IsValid() || Record.Transform.ContainsNaN() || Record.ActorSoftClass.IsNull()
            || static_cast<uint8>(Record.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)) { return false; }
        TSet<FName> ComponentNames;
        for (const auto& Component : Record.SavedComponents)
        {
            if (Component.ComponentName.IsNone() || ComponentNames.Contains(Component.ComponentName)
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
        if (Record.bOptional && !Record.bDestroyed)
        {
            UClass* Loaded = Record.ActorSoftClass.LoadSynchronous();
            if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return nullptr; }
            if (!Loaded)
            { UE_LOG(LogTemp, Warning, TEXT("Skipping unavailable optional save actor %s"), *Record.ActorName.ToString()); It.RemoveCurrent(); continue; }
        }
        for (int32 Index = Record.SavedComponents.Num() - 1; Index >= 0; --Index)
        {
            const auto& Component = Record.SavedComponents[Index];
            if (Component.bOptional && !Component.ComponentClass.IsNull())
            {
                UClass* Loaded = Component.ComponentClass.LoadSynchronous();
                if (Operation && !IsOperationOwnerCurrent(*Operation, Error)) { return nullptr; }
                if (!Loaded)
                { UE_LOG(LogTemp, Warning, TEXT("Skipping unavailable optional save component %s"), *Component.ComponentName.ToString()); Record.SavedComponents.RemoveAt(Index); }
            }
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
    if (bBusy || PendingSave || MissionTravelRequest.IsValid()) { return ESovSaveResult::Busy; }
    if (!SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return ESovSaveResult::InvalidSlot; }
    if (AccountNamespace.IsEmpty() || !bPlatformStorageOwnerAvailable)
    { Error = TEXT("The selected campaign's platform storage owner is unavailable."); return ESovSaveResult::MissingAccount; }
    ASovPlayerController* PC = Controller();
    if (!PC || !PC->HasAuthority() || PC->GetWorld()->GetNetMode() != NM_Standalone)
    { Error = TEXT("Campaign load requires the standalone local controller."); return ESovSaveResult::UnsafeState; }
    TGuardValue<bool> Mutation(bBusy, true);
    const FOperationOwner Owner = CaptureOperationOwner();
    const TWeakObjectPtr<ASovPlayerController> SourceController(PC);
    const TWeakObjectPtr<UWorld> SourceWorld(PC->GetWorld());
    int32 Bank; bool Damaged;
    PendingSave = ReadBest(Kind, Index, Bank, Damaged, Error, &Owner);
    if (!IsOperationOwnerCurrent(Owner, Error)) { PendingSave = nullptr; return OwnershipFailureResult(); }
    if (!PendingSave) { return Damaged ? ESovSaveResult::CorruptSave : ESovSaveResult::MissingSave; }
    if (Damaged && !bAcceptRecoveredBank)
    {
        const auto Header = PendingSave->Header; PendingSave = nullptr;
        Error = TEXT("A damaged bank was detected. A verified previous state is available; confirm recovery using its displayed mission and timestamp.");
        OnLoadCompleted.Broadcast(ESovSaveResult::RecoveryAvailable, Header, Error);
        return ESovSaveResult::RecoveryAvailable;
    }
    if (!ValidateEnvelope(PendingSave, true, Error, &Owner)) { PendingSave = nullptr; return ESovSaveResult::IncompatibleSave; }
    PendingNarrative = DecodeNarrative(PendingSave, Error, &Owner);
    if (!PendingNarrative) { PendingSave = nullptr; return ESovSaveResult::IncompatibleSave; }
    if (!IsOperationOwnerCurrent(Owner, Error) || !SourceController.IsValid() || !SourceWorld.IsValid()
        || Controller() != PC || PC->GetWorld() != SourceWorld.Get())
    { PendingSave = nullptr; PendingNarrative = nullptr; return ESovSaveResult::UnsafeState; }
    ResetRestoreOwner();
    PendingLoadOwner = Owner;
    bPendingWorldApplied = false; bPendingLoadFailed = false; PendingDestination.Reset();
    PendingLoadError.Reset(); PendingLoadRequest = FGuid::NewGuid();
    PendingLoadDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    const FString Destination = PendingSave->Header.MapPackage + TEXT("?SovCampaignSlotLoad=1?SovCampaignLoadRequest=")
        + PendingLoadRequest.ToString(EGuidFormats::Digits);
    AcknowledgeSaveFailure();
    if (!IsOperationOwnerCurrent(Owner, Error) || !SourceController.IsValid() || !SourceWorld.IsValid()
        || Controller() != PC || PC->GetWorld() != SourceWorld.Get())
    {
        PendingSave = nullptr; PendingNarrative = nullptr; PendingLoadRequest.Invalidate(); PendingLoadDeadline = 0;
        return ESovSaveResult::UnsafeState;
    }
    if (!SourceWorld->ServerTravel(Destination, true))
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
        && !UGameplayStatics::HasOption(Options, TEXT("SovCampaignTransition"))
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
    if (!IsPendingLoadOwnerCurrent(Error)) { return false; }
    const auto* GM = Cast<ASovCampaignGameMode>(World.GetAuthGameMode());
    const auto* Narrative = World.GetSubsystem<UNarrativeSaveSubsystem>();
    if (bPendingLoadFailed || !GM || !MatchesPendingLoadRequest(GM->OptionsString) || !GM->InitialMission
        || GM->InitialMission->Map.ToSoftObjectPath().GetLongPackageName() != PendingSave->Header.MapPackage
        || FPackageName::GetLongPackagePath(World.GetOutermost()->GetName()) + TEXT("/")
            + UGameplayStatics::GetCurrentLevelName(&World, true) != PendingSave->Header.MapPackage
        || GM->InitialMission->MissionId != PendingSave->Header.MissionId
        || FSoftObjectPath(GM->InitialMission) != PendingSave->Header.MissionDefinition
        || (Narrative && Narrative->DidInitialLoadFail()))
    { Error = TEXT("Saved destination initialization failed. Choose a last known-good autosave; the source banks remain intact."); return false; }
    return true;
}
void USovSaveSubsystem::ResetRestoreOwner()
{
    RestoreWorld.Reset(); RestoreController.Reset(); RestorePawn.Reset(); RestorePlayerState.Reset(); RestoreASC.Reset();
    PendingRestoreEpoch = 0; RestoreASCEpoch = 0; RestorePawnGeneration = 0;
}
bool USovSaveSubsystem::BindPendingRestore(ASovPlayerController* PC, uint64 RestoreEpoch, FString& Error)
{
    if (!IsLoadPending()) { return true; }
    if (!PC || PC != Controller() || !PC->GetWorld() || RestoreEpoch == 0
        || (PendingSave ? (PC->GetWorld() != PendingDestination.Get() || !ValidatePendingWorld(*PC->GetWorld(), Error))
                        : !ValidateMissionTravelWorld(*PC->GetWorld(), Error))) { return false; }
    if (PendingRestoreEpoch != 0)
    {
        if (MatchesRestoreOwner(PC, RestoreEpoch)) { return true; }
        Error = TEXT("The pending save belongs to a different managed pawn restoration."); return false;
    }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent());
    const auto* Pawn = Cast<ASovPlayerCharacterBase>(PC->GetPawn());
    if (!Pawn || !ASC || ASC->GetAvatarActor() != PC->GetPawn() || !PC->GetPlayerState<APlayerState>())
    { Error = TEXT("Managed restoration requires its exact initialized player, pawn and ASC."); return false; }
    RestoreWorld = PC->GetWorld(); RestoreController = PC; RestorePawn = PC->GetPawn(); RestoreASC = ASC;
    RestorePlayerState = PC->GetPlayerState<APlayerState>(); PendingRestoreEpoch = RestoreEpoch;
    RestoreASCEpoch = ASC->GetCombatActorInfoEpoch(); RestorePawnGeneration = Pawn->GetCharacterInitializationGeneration();
    return true;
}
bool USovSaveSubsystem::MatchesRestoreGenerations() const
{
    const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(RestoreASC.Get());
    const auto* Pawn = Cast<ASovPlayerCharacterBase>(RestorePawn.Get());
    return ASC && Pawn && ASC->GetCombatActorInfoEpoch() == RestoreASCEpoch
        && Pawn->GetCharacterInitializationGeneration() == RestorePawnGeneration;
}
bool USovSaveSubsystem::MatchesRestoreOwner(ASovPlayerController* PC, uint64 RestoreEpoch) const
{
    return PC && PC == Controller() && PC == RestoreController.Get() && !PC->IsActorBeingDestroyed()
        && PC->GetWorld() == RestoreWorld.Get()
        && PendingRestoreEpoch == RestoreEpoch && RestorePawn.IsValid() && PC->GetPawn() == RestorePawn.Get()
        && RestorePlayerState.IsValid() && PC->GetPlayerState<APlayerState>() == RestorePlayerState.Get()
        && RestoreASC.IsValid() && PC->GetAbilitySystemComponent() == RestoreASC.Get()
        && RestoreASC->GetAvatarActor() == RestorePawn.Get() && MatchesRestoreGenerations();
}
void USovSaveSubsystem::NotifyCampaignReady(ASovPlayerController* PC, bool bSucceeded, uint64 RestoreEpoch)
{
    // A reused pawn/ASC address does not identify the same managed restoration.
    if (IsLoadPending() && ((PendingRestoreEpoch != 0 && !MatchesRestoreOwner(PC, RestoreEpoch))
        || (bSucceeded && (PendingRestoreEpoch == 0 || !PC || PC->GetCampaignTransitionEpoch() != RestoreEpoch)))) { return; }
    NotifyMissionTravelReady(PC, bSucceeded);
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
    FString CompletionError = Error;
    if (bSucceeded && !IsPendingLoadOwnerCurrent(CompletionError)) { bSucceeded = false; }
    const auto Header = PendingSave->Header;
    if (bSucceeded) { PlaySeconds = Header.PlaySeconds; RejectedLoadWorld.Reset(); }
    else { RejectedLoadWorld = PendingDestination; }
    // Recovery owns the exact pending-load GUID; retire it before clearing that identity.
    CompleteMissionTravelRecovery(bSucceeded, CompletionError);
    PendingAutosaves.Reset();
    ResetRestoreOwner();
    PendingSave = nullptr; PendingNarrative = nullptr; PendingDestination.Reset();
    PendingLoadRequest.Invalidate(); PendingLoadDeadline = 0; PendingLoadError.Reset();
    bPendingWorldApplied = false; bPendingLoadFailed = !bSucceeded;
    // All ownership is released before observers may request a different recovery slot.
    OnLoadCompleted.Broadcast(bSucceeded ? ESovSaveResult::Success : ESovSaveResult::RecoveryAvailable, Header, CompletionError);
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
    if (bBusy || PendingSave || MissionTravelRequest.IsValid()) { return ESovSaveResult::Busy; }
    if (!bAwaitingFailureDecision || !FailedWrite) { Error = TEXT("No failed write is waiting for retry."); return ESovSaveResult::MissingSave; }
    FOperationOwner Owner = CaptureOperationOwner();
    Owner.bRetryOperation = true; Owner.RetrySnapshot = FailedWrite;
    if (!IsOperationOwnerCurrent(Owner, Error)) { return OwnershipFailureResult(); }
    // This is a deliberate new attempt, not automatic continuation of revoked I/O. The
    // original profile and local user must still own the retained immutable snapshot.
    if (FailedWriteOwner.Namespace != Owner.Namespace || FailedWriteOwner.LocalUser != Owner.LocalUser
        || FailedWriteOwner.SelectionEpoch != Owner.SelectionEpoch)
    { Error = TEXT("The failed snapshot belongs to a different profile selection. Continue without saving, then capture a new save."); return ESovSaveResult::MissingAccount; }
    TGuardValue<bool> Mutation(bBusy, true);
    TStrongObjectPtr<USovCampaignSaveGame> Candidate(FailedWrite);
    const auto Result = WriteEnvelope(Candidate.Get(), Error, &Owner);
    const auto Header = Candidate->Header;
    if (FailedWrite != Candidate.Get() || !bAwaitingFailureDecision)
    { Error = TEXT("The pending retry decision changed during storage work; its completion was not republished."); return ESovSaveResult::Busy; }
    if (Result == ESovSaveResult::Success && FailedWrite == Candidate.Get()) { FailedWrite = nullptr; AcknowledgeSaveFailure(); }
    ReportSave(Result, Header, Error);
    return Result;
}
bool USovSaveSubsystem::ConsumeAcknowledgedBoundary(ESovSaveBoundary Boundary, FName Id)
{
    FString Error;
    if (!IsRetainedOwnerCurrent(AcknowledgedOwner, Error)) { return false; }
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
        AcknowledgedOwner = FailedWriteOwner;
        AcknowledgedWorld = Controller() ? Controller()->GetWorld() : nullptr;
        AcknowledgmentExpiresAt = (bPlatformSuspended ? PlatformSuspendedAt : FPlatformTime::Seconds()) + 60.0;
    }
    const bool bReleasePause = bOwnPause;
    const TWeakObjectPtr<ASovPlayerController> PreviousPausedController = PausedController;
    bOwnPause = false; bAwaitingFailureDecision = false; PausedController.Reset();
    FailedWrite = nullptr; PendingAutosaves.Reset();
    if (bReleasePause && PreviousPausedController.IsValid()) { PreviousPausedController->ReleaseSystemPause(TEXT("SaveFailure")); }
}
bool USovSaveSubsystem::Tick(float DeltaSeconds)
{
    if (bPlatformSuspended || bBusy) { return true; }
    TickMissionTravelRecovery();
    if (bPlatformSuspended || bBusy || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return true; }
    // Core ticker may report the entire suspended interval on its first foreground frame.
    if (bDiscardPlatformResumeDelta) { bDiscardPlatformResumeDelta = false; return true; }
    ASovPlayerController* PC = Controller();
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
    if (PendingSave && !IsPendingLoadOwnerCurrent(Error))
    { CompletePendingLoad(false, Error); return true; }
    if (!PendingAutosaves.IsEmpty() && !IsOperationOwnerCurrent(PendingAutosaves.Last().Owner, Error)) { PendingAutosaves.Reset(); }
    const bool bEntryBoundary = !PendingAutosaves.IsEmpty() && (PendingAutosaves.Last().Kind == ESovSaveBoundary::ArenaEntry
        || PendingAutosaves.Last().Kind == ESovSaveBoundary::BossRetry);
    if (!PendingSave && !PendingAutosaves.IsEmpty() && !bAwaitingFailureDecision && CanCaptureInternal(Error, bEntryBoundary))
    {
        const FQueuedBoundary Boundary = PendingAutosaves.Last();
        if (!IsOperationOwnerCurrent(Boundary.Owner, Error)) { PendingAutosaves.Reset(); return true; }
        SovSavePolicy::Bank Slots[SovSavePolicy::AutoSlots];
        for (int32 Index = 0; Index < SovSavePolicy::AutoSlots; ++Index)
        { int32 Bank; bool Bad; auto* Save = ReadBest(ESovSaveSlotKind::Auto, Index, Bank, Bad, Error, &Boundary.Owner);
          if (!IsOperationOwnerCurrent(Boundary.Owner, Error)) { PendingAutosaves.Reset(); return true; }
          if (Save) { Slots[Index] = { true, Save->Header.Generation }; } }
        if (PendingAutosaves.IsEmpty() || PendingAutosaves.Last().Kind != Boundary.Kind
            || PendingAutosaves.Last().Id != Boundary.Id) { return true; }
        const auto Result = CaptureAndWrite(ESovSaveSlotKind::Auto, SovSavePolicy::OldestAuto(Slots), Boundary.Id, Error, bEntryBoundary, Boundary.Kind);
        if (Result == ESovSaveResult::Success && !PendingAutosaves.IsEmpty()
            && PendingAutosaves.Last().Kind == Boundary.Kind && PendingAutosaves.Last().Id == Boundary.Id) { PendingAutosaves.Reset(); }
    }
    return true;
}
