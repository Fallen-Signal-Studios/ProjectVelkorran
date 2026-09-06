// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovSaveSubsystem.h"
#include "Save/SovSavePolicy.h"
#include "Platform/SovPlatformServicesAdapter.h"
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
}
void USovSaveSubsystem::Deinitialize()
{
    UNarrativeSaveSubsystem::OnInitialSaveRequested.Remove(InitialSaveHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    AcknowledgeSaveFailure();
    ClearPendingOperation(); TravelRecoverySave = nullptr; Storage.Reset();
    Super::Deinitialize();
}
bool USovSaveSubsystem::SelectPlatformUser(const FString& Id, int32 LocalUserIndex, FString& Error)
{
    if (bPlatformSuspended || bBusy || PendingSave || bAwaitingFailureDecision)
    { Error = TEXT("Finish or cancel the save/load transaction before changing platform user."); return false; }
    if (Id.TrimStartAndEnd().IsEmpty() || LocalUserIndex < 0)
    { Error = TEXT("A stable platform account and nonnegative local user index are required."); return false; }
    if (bRequiresNativePlatformAuthorization && (!bHasNativePlatformAuthorization
        || Id != AuthorizedPlatformId || LocalUserIndex != AuthorizedPlatformLocalUser))
    { Error = TEXT("This platform account has not been authorized by the native account provider."); return false; }
    const FString NewNamespace = FMD5::HashAnsiString(*Id);
    if (AccountNamespace != NewNamespace || UserIndex != LocalUserIndex)
    {
        // Never stamp the outgoing user's live campaign records as another user's save.
        if (!AccountNamespace.IsEmpty() && !CanManagePlatformSaves(Error)) { return false; }
        if (!PersistPlatformProfileHint(NewNamespace, LocalUserIndex, Error)) { return false; }
        PendingAutosaves.Reset(); PlaySeconds = 0;
        AcknowledgedWorld.Reset(); AcknowledgmentExpiresAt = 0;
    }
    AccountNamespace = NewNamespace; UserIndex = LocalUserIndex;
    bPlatformStorageOwnerAvailable = true;
    return true;
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
    if (!Storage || LocalUserIndex < 0 || !IsOpaqueNamespace(Namespace))
    { Error = TEXT("Platform profile storage is unavailable."); return false; }
    FProfileHint Best; int32 BestBank = -1;
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; FProfileHint Hint;
        if (Storage->Read(ProfileHintSlot(LocalUserIndex, Bank), LocalUserIndex, Bytes)
            && DecodeProfileHint(Bytes, LocalUserIndex, Hint) && Hint.Generation > Best.Generation) { Best = Hint; BestBank = Bank; }
    }
    if (Best.Namespace == Namespace) { return true; }
    if (Best.Generation == MAX_int64) { Error = TEXT("Platform profile generation limit reached."); return false; }
    const TArray<uint8> Bytes = EncodeProfileHint(Namespace, LocalUserIndex, Best.Generation + 1);
    const FString Slot = ProfileHintSlot(LocalUserIndex, BestBank == 0 ? 1 : 0);
    TArray<uint8> Readback; FProfileHint Verified;
    if (!Storage->Write(Slot, LocalUserIndex, Bytes) || !Storage->Read(Slot, LocalUserIndex, Readback) || Readback != Bytes
        || !DecodeProfileHint(Readback, LocalUserIndex, Verified) || Verified.Namespace != Namespace)
    { Error = TEXT("Could not retain the selected profile for offline restart. Previous profile and campaign remain selected."); return false; }
    return true;
}
void USovSaveSubsystem::ObserveNativePlatformAccount(const FSovObservedPlatformAccount& Account)
{
    bRequiresNativePlatformAuthorization = !PLATFORM_DESKTOP || Account.bRequiresKnownStorageOwner;
    bHasNativePlatformAuthorization = Account.bIdentityKnown && !Account.StableId.IsEmpty() && Account.LocalUser >= 0
        && (!bRequiresNativePlatformAuthorization || Account.bStorageAccessAuthorized);
    AuthorizedPlatformId = bHasNativePlatformAuthorization ? Account.StableId : FString();
    AuthorizedPlatformLocalUser = bHasNativePlatformAuthorization ? Account.LocalUser : INDEX_NONE;
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
    if (bSuspended) { PlatformSuspendedAt = Now; return; }
    const double Elapsed = FMath::Max(0.0, Now - PlatformSuspendedAt);
    if (PendingLoadDeadline > 0) { PendingLoadDeadline += Elapsed; }
    if (AcknowledgmentExpiresAt > 0) { AcknowledgmentExpiresAt += Elapsed; }
    bDiscardPlatformResumeDelta = true;
    PlatformSuspendedAt = 0;
}
bool USovSaveSubsystem::CanManagePlatformSaves(FString& Error) const
{
    if (bPlatformSuspended || bBusy || PendingSave || bAwaitingFailureDecision)
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
    int32 Bank; bool Damaged;
    TStrongObjectPtr<USovCampaignSaveGame> Save(ReadBest(Kind, Index, Bank, Damaged, Error));
    // Do not turn a damaged local slot into an apparently empty cloud-import target.
    if (Damaged) { Error = TEXT("Recover the damaged local save before comparing cloud copies."); return false; }
    if (!Save.IsValid()) { Error.Reset(); return true; }
    if (!Storage->Read(BankName(Kind, Index, Bank), UserIndex, Bytes)
        || !ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error))
    { Bytes.Reset(); return false; }
    bExists = true; Error.Reset(); return true;
}
bool USovSaveSubsystem::ValidatePlatformSnapshot(const TArray<uint8>& Bytes, ESovSaveSlotKind Kind, int32 Index,
    FSovSaveSlotHeader& Header, FString& Error) const
{
    // Size is checked before Unreal deserializes provider-controlled bytes.
    if (Bytes.IsEmpty() || Bytes.Num() > 64 * 1024 * 1024)
    { Error = TEXT("Cloud save is empty or exceeds the supported 64 MiB envelope limit."); return false; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
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
    FSovSaveSlotHeader Header;
    if (!ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error)) { return ESovSaveResult::IncompatibleSave; }
    TStrongObjectPtr<USovCampaignSaveGame> Candidate(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!ValidateEnvelope(Candidate.Get(), true, Error) || !DecodeNarrative(Candidate.Get(), Error))
    { return ESovSaveResult::IncompatibleSave; }
    // Synchronous required-asset loads can pump events. Recheck admission/account after preflight.
    if (!CanManagePlatformSaves(Error) || !ValidateEnvelope(Candidate.Get(), false, Error)) { return ESovSaveResult::UnsafeState; }
    return CommitPlatformSnapshot(Bytes, ReviewedLocalBytes, Kind, Index, Error);
}
ESovSaveResult USovSaveSubsystem::CommitPlatformSnapshot(const TArray<uint8>& Bytes,
    const TArray<uint8>& ReviewedLocalBytes, ESovSaveSlotKind Kind, int32 Index, FString& Error)
{
    FSovSaveSlotHeader Header; bool Exists; TArray<uint8> Current;
    if (!ValidatePlatformSnapshot(Bytes, Kind, Index, Header, Error)) { return ESovSaveResult::IncompatibleSave; }
    if (!ExportPlatformSnapshot(Kind, Index, Current, Header, Exists, Error)) { return ESovSaveResult::CorruptSave; }
    if (Current != ReviewedLocalBytes)
    { Error = TEXT("Local save changed after review. Compare the copies again before importing."); return ESovSaveResult::Busy; }
    TGuardValue<bool> Mutation(bBusy, true);
    const FString ArchivePrefix = BankName(Kind, Index, 0) + TEXT("_CloudReview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto Preserve = [&](const FString& Suffix, const TArray<uint8>& Data)
    {
        if (Data.IsEmpty()) { return true; }
        TArray<uint8> Readback;
        return Storage->Write(ArchivePrefix + Suffix, UserIndex, Data)
            && Storage->Read(ArchivePrefix + Suffix, UserIndex, Readback) && Readback == Data;
    };
    if (!Preserve(TEXT("_Local"), Current) || !Preserve(TEXT("_Remote"), Bytes))
    { Error = TEXT("Could not durably preserve both reviewed copies. Local save banks were not changed."); return ESovSaveResult::WriteFailed; }
    TStrongObjectPtr<USovCampaignSaveGame> Save(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
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
    OutBank = -1; bDamaged = false;
    if (bPlatformSuspended || !Storage || !bPlatformStorageOwnerAvailable || !SovSavePolicy::ValidSlot(PolicyKind(Kind), Index) || AccountNamespace.IsEmpty()) { return nullptr; }
    TStrongObjectPtr<USovCampaignSaveGame> Banks[2];
    SovSavePolicy::Bank Valid[2];
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        TArray<uint8> Bytes; const FString Name = BankName(Kind, Index, Bank);
        if (!Storage->Exists(Name, UserIndex)) { continue; }
        if (Storage->Read(Name, UserIndex, Bytes))
        { Banks[Bank].Reset(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes))); }
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
    if (bPlatformSuspended) { Error = TEXT("Save writes are held while the application is suspended."); return ESovSaveResult::Busy; }
    if (!bPlatformStorageOwnerAvailable)
    { Error = TEXT("Original platform save owner is unavailable. The existing campaign and save banks were not changed."); return ESovSaveResult::MissingAccount; }
    if (!Storage || !Save) { Error = TEXT("Save storage is unavailable."); return ESovSaveResult::WriteFailed; }
    int32 OldBank = -1; bool Damaged = false;
    TStrongObjectPtr<USovCampaignSaveGame> Previous(ReadBest(Save->Header.Kind, Save->Header.SlotIndex, OldBank, Damaged, Error));
    int64 Generation = Previous.IsValid() ? Previous->Header.Generation : 0;
    // Generation is global within the rolling autosave group so rotation remains deterministic after restart.
    if (Save->Header.Kind == ESovSaveSlotKind::Auto)
    {
        for (int32 Index = 0; Index < SovSavePolicy::AutoSlots; ++Index)
        { int32 Bank; bool Bad; FString Ignored; auto* Other = ReadBest(ESovSaveSlotKind::Auto, Index, Bank, Bad, Ignored);
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
    const FString TargetName = BankName(Save->Header.Kind, Save->Header.SlotIndex, TargetBank);
    // Preserve a corrupt bank byte-for-byte for support before reusing its logical position.
    if (Storage->Exists(TargetName, UserIndex))
    {
        TArray<uint8> ExistingBytes;
        if (!Storage->Read(TargetName, UserIndex, ExistingBytes))
        { Error = TEXT("Existing save bank cannot be read safely; it was not overwritten."); return ESovSaveResult::WriteFailed; }
        TStrongObjectPtr<USovCampaignSaveGame> Existing(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(ExistingBytes)));
        FString ExistingError;
        if (!ValidateEnvelope(Existing.Get(), false, ExistingError)
            || Existing->Header.Kind != Save->Header.Kind || Existing->Header.SlotIndex != Save->Header.SlotIndex)
        {
            const FString RecoveryName = TargetName + TEXT("_Recovery_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
            TArray<uint8> RecoveryBytes;
            if (!Storage->Write(RecoveryName, UserIndex, ExistingBytes) || !Storage->Read(RecoveryName, UserIndex, RecoveryBytes)
                || RecoveryBytes != ExistingBytes)
            { Error = TEXT("Corrupt save could not be preserved for support; it was not overwritten."); return ESovSaveResult::WriteFailed; }
        }
    }
    if (!Storage->Write(TargetName, UserIndex, Bytes))
    { Error = TEXT("Platform save write failed. Previous good bank retained; free storage or continue without saving explicitly."); return ESovSaveResult::WriteFailed; }
    TArray<uint8> Readback;
    if (!Storage->Read(TargetName, UserIndex, Readback) || Readback != Bytes)
    { Error = TEXT("Save readback failed. Previous good bank retained; the new save is not confirmed."); return ESovSaveResult::ReadbackFailed; }
    TStrongObjectPtr<USovCampaignSaveGame> Verified(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Readback)));
    if (!ValidateEnvelope(Verified.Get(), false, Error)) { return ESovSaveResult::ReadbackFailed; }
    Error.Reset(); return ESovSaveResult::Success;
}
ESovSaveResult USovSaveSubsystem::CaptureAndWrite(ESovSaveSlotKind Kind, int32 Index, FName BoundaryId, FString& Error, bool bAllowEntrySuspension, ESovSaveBoundary Boundary)
{
    if (bBusy || PendingSave) { return ESovSaveResult::Busy; }
    if (bAwaitingFailureDecision) { return ESovSaveResult::AwaitingFailureDecision; }
    if (!SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return ESovSaveResult::InvalidSlot; }
    if (AccountNamespace.IsEmpty() || !bPlatformStorageOwnerAvailable)
    { Error = TEXT("The selected campaign's platform storage owner is unavailable."); return ESovSaveResult::MissingAccount; }
    if (!CanCaptureInternal(Error, bAllowEntrySuspension)) { return ESovSaveResult::UnsafeState; }
    TGuardValue<bool> Mutation(bBusy, true);
    ASovPlayerController* PC = Controller(); APawn* Pawn = PC->GetPawn();
    auto* Mission = PC->GetCampaignState()->GetActiveMission();
    UNarrativeSave* Snapshot = nullptr;
    if (!PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>()->CaptureSaveObject(Snapshot))
    { Error = TEXT("Narrative actor/component serialization failed; previous save retained."); return ESovSaveResult::CaptureFailed; }
    TStrongObjectPtr<UNarrativeSave> KeepSnapshot(Snapshot);
    if (Controller() != PC || PC->GetPawn() != Pawn || PC->GetCampaignState()->GetActiveMission() != Mission || !CanCaptureInternal(Error, bAllowEntrySuspension))
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
    TArray<FSovSaveSlotHeader> Result;
    for (int32 Kind = 0; Kind < 3; ++Kind)
    {
        const auto Type = static_cast<ESovSaveSlotKind>(Kind);
        const int32 Count = Type == ESovSaveSlotKind::Manual ? SovSavePolicy::ManualSlots : Type == ESovSaveSlotKind::Auto ? SovSavePolicy::AutoSlots : 1;
        for (int32 Index = 0; Index < Count; ++Index)
        { int32 Bank; bool Bad; FString Error; if (auto* Save = ReadBest(Type, Index, Bank, Bad, Error)) { Result.Add(Save->Header); } }
    }
    return Result;
}
bool USovSaveSubsystem::FindRecoveryAutosave(FSovSaveSlotHeader& Slot)
{
    bool Found = false;
    for (const auto& Header : ListSlots())
    {
        if (Header.Kind == ESovSaveSlotKind::Auto && (!Found || Header.Generation > Slot.Generation))
        {
            int32 Bank; bool Bad; FString Error;
            // Required asset preflight can synchronously load packages and collect garbage.
            TStrongObjectPtr<USovCampaignSaveGame> Save(ReadBest(Header.Kind, Header.SlotIndex, Bank, Bad, Error));
            if (ValidateEnvelope(Save.Get(), true, Error) && DecodeNarrative(Save.Get(), Error)) { Slot = Header; Found = true; }
        }
    }
    return Found;
}
UNarrativeSave* USovSaveSubsystem::DecodeNarrative(USovCampaignSaveGame* Save, FString& Error) const
{
    auto* Snapshot = Save ? Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Save->NarrativePayload)) : nullptr;
    TStrongObjectPtr<UNarrativeSave> KeepSnapshot(Snapshot); // Required/optional synchronous asset loads may collect unreachable objects.
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
    const FString LoadOwner = AccountNamespace; const int32 LoadUser = UserIndex;
    TStrongObjectPtr<ASovPlayerController> KeepLoadController(PC);
    const TWeakObjectPtr<UWorld> LoadSource(PC->GetWorld());
    int32 Bank; bool Damaged;
    PendingSave = ReadBest(Kind, Index, Bank, Damaged, Error);
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
    if (AccountNamespace != LoadOwner || UserIndex != LoadUser || !IsPlatformStorageOwnerAvailable()
        || bPlatformSuspended || !IsValid(PC) || PC != Controller() || PC->GetWorld() != LoadSource.Get())
    { ClearPendingOperation(); Error = TEXT("Load ownership changed during save preflight."); return ESovSaveResult::UnsafeState; }
    bPendingWorldApplied = false; bPendingLoadFailed = false; PendingDestination.Reset();
    PendingAccount = AccountNamespace; PendingUser = UserIndex; PendingSource = PC->GetWorld();
    PendingLoadError.Reset(); PendingLoadRequest = FGuid::NewGuid(); PendingOperationId = PendingLoadRequest;
    PendingLoadDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    const FString Destination = PendingSave->Header.MapPackage + TEXT("?SovCampaignSlotLoad=1?SovCampaignLoadRequest=")
        + PendingLoadRequest.ToString(EGuidFormats::Digits);
    const FGuid Request = PendingLoadRequest;
    AcknowledgeSaveFailure();
    if (PendingLoadRequest != Request || !OwnsPendingAccount() || !IsValid(PC) || PC != Controller() || PC->GetWorld() != LoadSource.Get())
    {
        if (PendingLoadRequest == Request) { ClearPendingOperation(); }
        Error = TEXT("Load ownership changed while releasing the save-failure pause."); return ESovSaveResult::UnsafeState;
    }
    ArmTravelFailureHook(PendingLoadRequest);
    if (!PC->GetWorld()->ServerTravel(Destination, true))
    {
        ClearPendingOperation();
        Error = TEXT("Saved-map travel was rejected; current world retained."); return ESovSaveResult::TravelFailed;
    }
    // Acceptance starts asynchronous map/managed-pawn restoration. Success notification occurs only at CharacterReady.
    return ESovSaveResult::LoadStarted;
}
void USovSaveSubsystem::ResolveInitialSave(UWorld& World, UNarrativeSave*& Snapshot, bool& bOverride)
{
    if (World.GetGameInstance() != GetGameInstance()) { return; }
    const AGameModeBase* GM = World.GetAuthGameMode();
    if (!GM) { return; }
    if (UGameplayStatics::HasOption(GM->OptionsString, TEXT("SovCampaignTransition")))
    {
        FString Error;
        if (!MatchesPendingLoadRequest(GM->OptionsString) || !ValidatePendingWorld(World, Error))
        { bOverride = true; Snapshot = nullptr; return; }
        if (bPendingWorldApplied)
        {
            bPendingLoadFailed = true; PendingLoadError = TEXT("The destination attempted to initialize its travel world twice.");
            bOverride = true; Snapshot = nullptr; return;
        }
        PendingDestination = &World; bPendingWorldApplied = true;
        return; // New destination world, with only the request-bound player record staged by GameMode.
    }
    if (!UGameplayStatics::HasOption(GM->OptionsString, TEXT("SovCampaignSlotLoad"))) { return; }
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
    const bool bMissionURL = UGameplayStatics::HasOption(Options, TEXT("SovCampaignTransition"));
    const bool bSlotURL = UGameplayStatics::HasOption(Options, TEXT("SovCampaignSlotLoad"));
    return PendingSave && PendingLoadRequest.IsValid() && bMissionURL != bSlotURL
        && bMissionURL == bPendingMissionTravel
        && FGuid::ParseExact(UGameplayStatics::ParseOption(Options, TEXT("SovCampaignLoadRequest")), EGuidFormats::Digits, Request)
        && Request == PendingLoadRequest;
}
bool USovSaveSubsystem::OwnsPendingAccount() const
{
    return PendingSave && IsPlatformStorageOwnerAvailable() && !bPlatformSuspended
        && PendingAccount == AccountNamespace && PendingUser == UserIndex
        && PendingSave->Header.AccountNamespace == PendingAccount;
}
bool USovSaveSubsystem::ValidatePendingWorld(UWorld& World, FString& Error) const
{
    const AGameModeBase* Mode = World.GetAuthGameMode();
    const bool bRequested = Mode && (UGameplayStatics::HasOption(Mode->OptionsString, TEXT("SovCampaignSlotLoad"))
        || UGameplayStatics::HasOption(Mode->OptionsString, TEXT("SovCampaignTransition")));
    if (bRequested && !MatchesPendingLoadRequest(Mode->OptionsString))
    { Error = TEXT("This campaign travel request has expired or was replaced. Select a valid recovery save."); return false; }
    if (bPendingLoadFailed && RejectedLoadWorld.Get() == &World)
    { Error = TEXT("This campaign world failed restoration; choose a valid recovery save."); return false; }
    if (!PendingSave) { return true; }
    const auto* GM = Cast<ASovCampaignGameMode>(Mode);
    const auto* Narrative = World.GetSubsystem<UNarrativeSaveSubsystem>();
    const FSoftObjectPath ExpectedMission = bPendingMissionTravel ? PendingTravelMission : PendingSave->Header.MissionDefinition;
    const FString ExpectedMap = bPendingMissionTravel ? PendingTravelMap : PendingSave->Header.MapPackage;
    // The world package, mission asset, account and phase must all name the accepted request.
    FString ActualMap = World.GetOutermost()->GetName();
    if (World.WorldType == EWorldType::PIE && !World.StreamingLevelsPrefix.IsEmpty())
    {
        FString ShortName = FPackageName::GetShortName(ActualMap);
        ShortName.RemoveFromStart(World.StreamingLevelsPrefix);
        ActualMap = FPackageName::GetLongPackagePath(ActualMap) + TEXT("/") + ShortName;
    }
    if ((PendingDestination.IsValid() && PendingDestination.Get() != &World) || !bRequested || !OwnsPendingAccount() || bPendingLoadFailed || !GM || !GM->InitialMission
        || FSoftObjectPath(GM->InitialMission) != ExpectedMission
        || GM->InitialMission->Map.ToSoftObjectPath().GetLongPackageName() != ExpectedMap
        || ActualMap != ExpectedMap
        || (Narrative && Narrative->DidInitialLoadFail()))
    { Error = TEXT("Campaign destination, storage owner or snapshot initialization failed. The origin save banks remain intact."); return false; }
    return true;
}
bool USovSaveSubsystem::BindPendingRestore(ASovPlayerController* PC, uint64 RestoreEpoch, FString& Error)
{
    if (!PendingSave) { return true; }
    if (!PC || PC != Controller() || !PC->GetWorld() || PC->GetWorld() != PendingDestination.Get()
        || !ValidatePendingWorld(*PC->GetWorld(), Error)) { return false; }
    if (PendingRestoreEpoch != 0)
    {
        if (MatchesRestoreOwner(PC, RestoreEpoch)) { return true; }
        Error = TEXT("The pending save belongs to a different managed pawn restoration."); return false;
    }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent());
    const auto* Pawn = Cast<ASovPlayerCharacterBase>(PC->GetPawn());
    if (!Pawn || !ASC || ASC->GetAvatarActor() != PC->GetPawn() || !PC->GetPlayerState<APlayerState>())
    { Error = TEXT("Managed restoration requires its exact initialized player, pawn and ASC."); return false; }
    RestoreController = PC; RestorePawn = PC->GetPawn(); RestoreASC = ASC;
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
        && PC->GetWorld() == PendingDestination.Get()
        && PendingRestoreEpoch == RestoreEpoch && RestorePawn.IsValid() && PC->GetPawn() == RestorePawn.Get()
        && RestorePlayerState.IsValid() && PC->GetPlayerState<APlayerState>() == RestorePlayerState.Get()
        && RestoreASC.IsValid() && PC->GetAbilitySystemComponent() == RestoreASC.Get()
        && RestoreASC->GetAvatarActor() == RestorePawn.Get() && MatchesRestoreGenerations();
}
void USovSaveSubsystem::NotifyCampaignReady(ASovPlayerController* PC, bool bSucceeded, uint64 RestoreEpoch)
{
    if (!PendingSave || !MatchesRestoreOwner(PC, RestoreEpoch) || !PC->GetWorld()->GetAuthGameMode()
        || !MatchesPendingLoadRequest(PC->GetWorld()->GetAuthGameMode()->OptionsString)) { return; }
    FString Error;
    const auto* Pawn = Cast<ASovPlayerCharacterBase>(PC->GetPawn());
    const bool Good = bSucceeded && (bPendingMissionTravel || bPendingWorldApplied)
        && PC->GetCampaignTransitionEpoch() == RestoreEpoch && ValidatePendingWorld(*PC->GetWorld(), Error)
        && Pawn && Pawn->IsCharacterReady();
    if (Good) { CompletePendingLoad(true, FString()); return; }
    // Failure/recovery may travel again. Wait until managed initialization and delegates unwind.
    bPendingLoadFailed = true;
    PendingLoadError = Error.IsEmpty() ? TEXT("Managed campaign restoration failed.") : Error;
}
void USovSaveSubsystem::ClearPendingOperation()
{
    if (ASovPlayerController* Source = TravelSourceController.Get())
    {
        if (Source->GetCampaignTransitionEpoch() == TravelSourceEpoch && Source->PendingTravelOperationId == PendingOperationId)
        { Source->PendingTravelOperationId.Invalidate(); Source->PendingTravelOriginGeneration = 0; }
    }
    if (GEngine) { GEngine->OnTravelFailure().Remove(TravelFailureHandle); }
    TravelFailureHandle.Reset();
    PendingSave = nullptr; PendingNarrative = nullptr; PendingTravelRecords = FNarrativeSavePlayer();
    PendingDestination.Reset(); PendingSource.Reset(); PendingLoadRequest.Invalidate(); PendingOperationId.Invalidate();
    PendingLoadDeadline = 0; PendingLoadError.Reset(); PendingAccount.Reset(); PendingUser = INDEX_NONE;
    PendingTravelMission.Reset(); PendingTravelMap.Reset(); bPendingMissionTravel = false; bRecoveringMissionTravel = false;
    TravelSourceController.Reset(); TravelSourcePawn.Reset(); TravelSourcePlayerState.Reset(); TravelSourceASC.Reset(); TravelSourceEpoch = 0; TravelSourceASCEpoch = 0; TravelSourcePawnGeneration = 0;
    RestoreController.Reset(); RestorePawn.Reset(); RestorePlayerState.Reset(); RestoreASC.Reset(); PendingRestoreEpoch = 0; RestoreASCEpoch = 0; RestorePawnGeneration = 0;
    bPendingWorldApplied = false; bPendingLoadFailed = false;
}
void USovSaveSubsystem::CompletePendingLoad(bool bSucceeded, const FString& Error)
{
    if (!PendingSave) { return; }
    const auto Header = PendingSave->Header;
    const bool bRecovered = bRecoveringMissionTravel;
    if (bSucceeded) { PlaySeconds = Header.PlaySeconds; RejectedLoadWorld.Reset(); TravelRecoverySave = nullptr; }
    else { RejectedLoadWorld = PendingDestination; }
    PendingAutosaves.Reset();
    ClearPendingOperation();
    bPendingLoadFailed = !bSucceeded;
    FString Message = Error;
    if (bSucceeded && bRecovered)
    { Message = TravelRecoveryReason + TEXT(" Returned to the verified origin checkpoint."); }
    // Terminal ownership is released before observers can retry. Recovery is a successful LOAD,
    // never a claim that the failed destination mission was entered.
    OnLoadCompleted.Broadcast(bSucceeded ? ESovSaveResult::Success : ESovSaveResult::RecoveryAvailable, Header, Message);
}
bool USovSaveSubsystem::OwnsTravelSource() const
{
    ASovPlayerController* PC = TravelSourceController.Get();
    const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(TravelSourceASC.Get());
    const auto* Pawn = Cast<ASovPlayerCharacterBase>(TravelSourcePawn.Get());
    return ASC && Pawn && ASC->GetCombatActorInfoEpoch() == TravelSourceASCEpoch
        && Pawn->GetCharacterInitializationGeneration() == TravelSourcePawnGeneration && OwnsPendingAccount() && bPendingMissionTravel && PC && PC == Controller() && !PC->IsActorBeingDestroyed()
        && PC->GetWorld() == PendingSource.Get() && PC->GetCampaignTransitionEpoch() == TravelSourceEpoch
        && PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Travelling
        && TravelSourcePawn.IsValid() && PC->GetPawn() == TravelSourcePawn.Get()
        && TravelSourcePlayerState.IsValid() && PC->GetPlayerState<APlayerState>() == TravelSourcePlayerState.Get()
        && TravelSourceASC.IsValid() && PC->GetAbilitySystemComponent() == TravelSourceASC.Get()
        && TravelSourceASC->GetAvatarActor() == TravelSourcePawn.Get();
}
bool USovSaveSubsystem::PrepareMissionTravel(ASovPlayerController* PC, USovCampaignDefinition* Destination,
    uint64 SourceEpoch, FString& TravelURL, FString& Error)
{
    TravelURL.Reset();
    if (bBusy || PendingSave || bAwaitingFailureDecision || !PC || PC != Controller() || !Destination
        || !PC->HasAuthority() || !PC->GetWorld() || PC->GetWorld()->GetNetMode() != NM_Standalone
        || !IsPlatformStorageOwnerAvailable() || bPlatformSuspended)
    { Error = TEXT("Finish the current save operation before mission travel."); return false; }
    TGuardValue<bool> Mutation(bBusy, true);
    const FString Owner = AccountNamespace; const int32 LocalUser = UserIndex;
    APawn* SourcePawn = PC->GetPawn(); APlayerState* SourcePS = PC->GetPlayerState<APlayerState>();
    auto* SourceASC = Cast<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent());
    auto* SourcePlayer = Cast<ASovPlayerCharacterBase>(SourcePawn);
    if (!IsValid(SourcePawn) || !IsValid(SourcePS) || !SourceASC || !SourcePlayer)
    { Error = TEXT("Mission travel requires its initialized protagonist owners."); return false; }
    TStrongObjectPtr<ASovPlayerController> KeepController(PC);
    TStrongObjectPtr<APawn> KeepSourcePawn(SourcePawn);
    TStrongObjectPtr<APlayerState> KeepSourcePS(SourcePS);
    TStrongObjectPtr<UNarrativeAbilitySystemComponent> KeepSourceASC(SourceASC);
    TStrongObjectPtr<USovCampaignDefinition> KeepDestination(Destination);
    const uint64 SourceASCEpoch = SourceASC->GetCombatActorInfoEpoch();
    const int32 SourcePawnGeneration = SourcePlayer->GetCharacterInitializationGeneration();
    USovCampaignDefinition* OriginMission = PC->GetCampaignState()->GetActiveMission();
    int32 Bank; bool Damaged;
    TStrongObjectPtr<USovCampaignSaveGame> Origin(ReadBest(ESovSaveSlotKind::Checkpoint, 0, Bank, Damaged, Error));
    if (!Origin.IsValid() || Damaged || !OriginMission
        || Origin->Header.MissionDefinition != FSoftObjectPath(OriginMission)
        || Origin->Header.BoundaryKind != ESovSaveBoundary::LongTransition || Origin->Header.BoundaryId != Destination->MissionId
        || Origin->Header.ActiveProtagonist != PC->GetCampaignState()->GetActiveProtagonist()
        || !ValidateEnvelope(Origin.Get(), true, Error))
    { Error = TEXT("Mission travel needs a verified origin checkpoint for this transition. Retry the checkpoint write before leaving."); return false; }
    TStrongObjectPtr<UNarrativeSave> OriginNarrative(DecodeNarrative(Origin.Get(), Error));
    if (!OriginNarrative.IsValid()) { return false; }
    // Required asset preflight may dispatch code. Install no transaction for a replacement owner.
    if (!IsValid(PC) || PC != Controller() || PC->GetPawn() != SourcePawn || !IsValid(Destination) || PC->PendingTravelMission != Destination
        || PC->GetPlayerState<APlayerState>() != SourcePS || PC->GetAbilitySystemComponent() != SourceASC
        || !IsValid(SourcePawn) || !IsValid(SourcePS) || !IsValid(SourceASC)
        || SourceASC->GetCombatActorInfoEpoch() != SourceASCEpoch || SourcePlayer->GetCharacterInitializationGeneration() != SourcePawnGeneration
        || SourceASC->GetAvatarActor() != SourcePawn || PC->GetCampaignTransitionEpoch() != SourceEpoch
        || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Travelling
        || AccountNamespace != Owner || UserIndex != LocalUser || !IsPlatformStorageOwnerAvailable() || bPlatformSuspended)
    { Error = TEXT("Travel ownership changed during checkpoint preflight."); return false; }
    PendingSave = Origin.Get(); PendingNarrative = OriginNarrative.Get();
    PendingAccount = Owner; PendingUser = LocalUser;
    PendingOperationId = FGuid::NewGuid(); PendingLoadRequest = PendingOperationId;
    PendingSource = PC->GetWorld(); TravelSourceController = PC; TravelSourcePawn = SourcePawn;
    TravelSourcePlayerState = SourcePS; TravelSourceASC = SourceASC; TravelSourceEpoch = SourceEpoch;
    TravelSourceASCEpoch = SourceASCEpoch; TravelSourcePawnGeneration = SourcePawnGeneration;
    PendingTravelMission = FSoftObjectPath(Destination); PendingTravelMap = Destination->Map.ToSoftObjectPath().GetLongPackageName();
    bPendingMissionTravel = true; bPendingLoadFailed = false; bPendingWorldApplied = false;
    PendingLoadDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    PC->PendingTravelOperationId = PendingOperationId;
    PC->PendingTravelOriginGeneration = Origin->Header.Generation;
    const FGuid Request = PendingLoadRequest;
    auto* Narrative = PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
    const FString Slot = FString(ASovPlayerController::TravelSaveSlot()) + TEXT("_") + Owner;
    const auto StillSource = [this, Request]() { return PendingLoadRequest == Request && OwnsTravelSource(); };
    // The transient player slot remains durable, while only the exact request may consume its readback.
    // No destination performs an unversioned account-slot lookup.
    if (!Narrative || !Narrative->CreatePlayerOnlySaveInSlot(PC, Slot, LocalUser, StillSource)
        || !StillSource() || !Narrative->ReadPlayerOnlySave(Slot, PendingTravelRecords, LocalUser, StillSource) || !StillSource())
    {
        if (PendingLoadRequest == Request) { ClearPendingOperation(); }
        Error = TEXT("Travel player record failed validation or its owner changed; origin checkpoint retained."); return false;
    }
    TravelRecoverySave = Origin.Get(); TravelRecoveryUser = LocalUser;
    PendingAutosaves.Reset();
    ArmTravelFailureHook(Request);
    TravelURL = PendingTravelMap + TEXT("?SovCampaignTransition=1?SovCampaignLoadRequest=") + Request.ToString(EGuidFormats::Digits);
    return true;
}
bool USovSaveSubsystem::CanCommitMissionTravel(ASovPlayerController* PC, uint64 SourceEpoch) const
{
    return PC == TravelSourceController.Get() && SourceEpoch == TravelSourceEpoch && OwnsTravelSource() && !bPendingLoadFailed;
}
bool USovSaveSubsystem::ValidateRestoredTravelIdentity(const ASovPlayerController* PC) const
{
    return PC && bPendingMissionTravel && OwnsPendingAccount() && PendingOperationId.IsValid()
        && PC->PendingTravelOperationId == PendingOperationId
        && PC->PendingTravelOriginGeneration == PendingSave->Header.Generation
        && FSoftObjectPath(PC->PendingTravelMission) == PendingTravelMission;
}
void USovSaveSubsystem::RejectMissionTravel(ASovPlayerController* PC, uint64 SourceEpoch)
{
    if (!bPendingMissionTravel || TravelSourceController.Get() != PC || TravelSourceEpoch != SourceEpoch) { return; }
    ClearPendingOperation(); // Immediate rejection retains the live origin and its durable checkpoint.
    TravelRecoverySave = nullptr;
}
bool USovSaveSubsystem::ReadMissionTravelRecords(UWorld& World, FNarrativeSavePlayer& Records, FString& Error)
{
    Records = FNarrativeSavePlayer();
    if (!bPendingMissionTravel || !ValidatePendingWorld(World, Error) || !PendingTravelRecords.IsValid()
        || (PendingDestination.IsValid() && PendingDestination.Get() != &World))
    { if (Error.IsEmpty()) { Error = TEXT("No matching campaign travel player record is available."); } return false; }
    PendingDestination = &World;
    Records = PendingTravelRecords;
    return true;
}
void USovSaveSubsystem::ArmTravelFailureHook(const FGuid& Request)
{
    if (!GEngine) { return; }
    GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    TravelFailureHandle = GEngine->OnTravelFailure().AddWeakLambda(this,
        [this, Request](UWorld* World, ETravelFailure::Type Failure, const FString& Error)
        { HandleTravelFailure(Request, World, FString::Printf(TEXT("Unreal travel failure %d: %s"), static_cast<int32>(Failure), *Error)); });
}
void USovSaveSubsystem::HandleTravelFailure(const FGuid& Request, UWorld* World, const FString& Error)
{
    // The engine event has no URL. Restrict it to the currently owned phase and known worlds;
    // a copied delegate from an older request cannot fail a later request.
    if (!PendingSave || Request != PendingLoadRequest || !World || World->GetGameInstance() != GetGameInstance()
        || (World != PendingSource.Get() && World != PendingDestination.Get())) { return; }
    bPendingLoadFailed = true; PendingLoadError = Error;
    // Never start another travel from inside Unreal's failure broadcast.
}
bool USovSaveSubsystem::HasTravelRecovery() const
{
    return TravelRecoverySave && !PendingSave && IsPlatformStorageOwnerAvailable() && !bPlatformSuspended
        && TravelRecoverySave->Header.AccountNamespace == AccountNamespace && TravelRecoveryUser == UserIndex;
}
bool USovSaveSubsystem::RequestPendingMap(UWorld& World, FString& Error)
{
    const FString URL = PendingSave->Header.MapPackage + TEXT("?SovCampaignSlotLoad=1?SovCampaignLoadRequest=")
        + PendingLoadRequest.ToString(EGuidFormats::Digits);
    PendingSource = &World;
    ArmTravelFailureHook(PendingLoadRequest);
    bool bAccepted = false;
#if WITH_AUTOMATION_TESTS
    if (TestTravelRequest) { bAccepted = TestTravelRequest(World, URL); }
    else
#endif
    { bAccepted = World.ServerTravel(URL, true); }
    if (!bAccepted)
    { Error = TEXT("Unreal rejected origin checkpoint recovery. Retry recovery or select a compatible save."); return false; }
    return true;
}
bool USovSaveSubsystem::StartOriginRecovery(FString& Error)
{
    if (!PendingSave || !bPendingMissionTravel || !OwnsPendingAccount())
    { Error = TEXT("Origin recovery is waiting for the original platform account. Reconnect it and retry recovery."); return false; }
    UWorld* World = GetWorld();
    if (!World && PendingSource.IsValid()) { World = PendingSource.Get(); }
    if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_Standalone)
    { Error = TEXT("Origin recovery requires a usable standalone world. Reopen the campaign checkpoint from the front end."); return false; }
    TravelRecoveryReason = Error;
    // Cancel only this phase's queued request. Never erase an unrelated replacement URL.
    if (World->NextURL.Contains(PendingLoadRequest.ToString(EGuidFormats::Digits)))
    { World->NextURL.Reset(); World->NextSwitchCountdown = 0.f; }
    bPendingMissionTravel = false; bRecoveringMissionTravel = true;
    PendingLoadRequest = FGuid::NewGuid(); // Operation ID remains stable; old destination callbacks now expire.
    PendingTravelRecords = FNarrativeSavePlayer(); PendingDestination.Reset();
    RestoreController.Reset(); RestorePawn.Reset(); RestoreASC.Reset(); RestorePlayerState.Reset(); PendingRestoreEpoch = 0; RestoreASCEpoch = 0; RestorePawnGeneration = 0;
    bPendingWorldApplied = false; bPendingLoadFailed = false; PendingLoadError.Reset();
    PendingLoadDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    return RequestPendingMap(*World, Error);
}
ESovSaveResult USovSaveSubsystem::RetryTravelRecovery(FString& Error)
{
    if (bBusy || PendingSave || bAwaitingFailureDecision) { return ESovSaveResult::Busy; }
    if (!HasTravelRecovery())
    { Error = TEXT("Restore the original account to retry its retained origin checkpoint."); return ESovSaveResult::MissingAccount; }
    TGuardValue<bool> Mutation(bBusy, true);
    TStrongObjectPtr<USovCampaignSaveGame> Origin(TravelRecoverySave);
    const int32 OriginUser = TravelRecoveryUser;
    if (!ValidateEnvelope(Origin.Get(), true, Error)) { return ESovSaveResult::IncompatibleSave; }
    TStrongObjectPtr<UNarrativeSave> Snapshot(DecodeNarrative(Origin.Get(), Error));
    UWorld* World = GetWorld();
    if (!Snapshot.IsValid() || !World || World->GetNetMode() != NM_Standalone || !HasTravelRecovery()
        || TravelRecoverySave != Origin.Get() || UserIndex != OriginUser)
    { Error = TEXT("Origin recovery preflight failed or its account changed."); return ESovSaveResult::UnsafeState; }
    PendingSave = Origin.Get(); PendingNarrative = Snapshot.Get(); PendingAccount = AccountNamespace; PendingUser = UserIndex;
    PendingOperationId = FGuid::NewGuid(); PendingLoadRequest = FGuid::NewGuid(); bRecoveringMissionTravel = true;
    bPendingLoadFailed = false; bPendingWorldApplied = false; PendingLoadDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    if (!RequestPendingMap(*World, Error)) { CompletePendingLoad(false, Error); return ESovSaveResult::TravelFailed; }
    return ESovSaveResult::LoadStarted;
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
    const auto Result = WriteEnvelope(FailedWrite, Error);
    const auto Header = FailedWrite->Header;
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
    if (bPlatformSuspended || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f || bBusy) { return true; }
    // Core ticker may report the entire suspended interval on its first foreground frame.
    if (bDiscardPlatformResumeDelta) { bDiscardPlatformResumeDelta = false; return true; }
    ASovPlayerController* PC = Controller();
    if (PC && PC->GetPawn() && PC->GetCampaignState()->GetActiveMission() && !UGameplayStatics::IsGamePaused(PC))
    { PlaySeconds += DeltaSeconds; }
    if (PendingSave && !OwnsPendingAccount() && !bPendingLoadFailed)
    { bPendingLoadFailed = true; PendingLoadError = TEXT("The original campaign storage owner is unavailable. Reconnect it before retrying recovery."); }
    if (PendingSave && (bPendingWorldApplied || bPendingMissionTravel) && !bPendingLoadFailed && PendingDestination.IsValid())
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
        FString Error = PendingLoadError.IsEmpty()
            ? TEXT("Campaign travel initialization timed out.") : PendingLoadError;
        if (bPendingMissionTravel && StartOriginRecovery(Error)) { return true; }
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
