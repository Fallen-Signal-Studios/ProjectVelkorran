// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovSaveSubsystem.h"
#include "Save/SovSavePolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Campaign/SovCampaignDefinition.h"
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
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
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
}

void USovSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Storage = MakeUnique<FPlatformSaveStorage>();
    // Offline profiles remain scoped by the OS/platform save API. Signed-in frontends
    // replace this explicit local profile before selecting campaign slots.
    FString Error; SelectPlatformUser(TEXT("Offline.LocalProfile.0"), 0, Error);
    InitialSaveHandle = UNarrativeSaveSubsystem::OnInitialSaveRequested.AddUObject(this, &USovSaveSubsystem::ResolveInitialSave);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USovSaveSubsystem::Tick));
}
void USovSaveSubsystem::Deinitialize()
{
    UNarrativeSaveSubsystem::OnInitialSaveRequested.Remove(InitialSaveHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    AcknowledgeSaveFailure();
    PendingSave = nullptr; PendingNarrative = nullptr; Storage.Reset();
    Super::Deinitialize();
}
bool USovSaveSubsystem::SelectPlatformUser(const FString& Id, int32 LocalUserIndex, FString& Error)
{
    if (bBusy || PendingSave || bAwaitingFailureDecision)
    { Error = TEXT("Finish or cancel the save/load transaction before changing platform user."); return false; }
    if (Id.TrimStartAndEnd().IsEmpty() || LocalUserIndex < 0)
    { Error = TEXT("A stable platform account and nonnegative local user index are required."); return false; }
    const FString NewNamespace = FMD5::HashAnsiString(*Id);
    if (!AccountNamespace.IsEmpty() && AccountNamespace != NewNamespace)
    {
        // Never stamp the outgoing user's live campaign records as another user's save.
        if (const ASovPlayerController* PC = Controller(); PC && PC->GetCampaignState()->GetActiveMission())
        { Error = TEXT("Return to the front end before changing the campaign's platform account."); return false; }
        PendingAutosaves.Reset(); PlaySeconds = 0;
    }
    AccountNamespace = NewNamespace; UserIndex = LocalUserIndex;
    return true;
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
    if (!Storage || !SovSavePolicy::ValidSlot(PolicyKind(Kind), Index) || AccountNamespace.IsEmpty()) { return nullptr; }
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
        if (!ValidateEnvelope(Existing.Get(), false, ExistingError))
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
    if (AccountNamespace.IsEmpty()) { return ESovSaveResult::MissingAccount; }
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
            int32 Bank; bool Bad; FString Error; auto* Save = ReadBest(Header.Kind, Header.SlotIndex, Bank, Bad, Error);
            if (ValidateEnvelope(Save, true, Error) && DecodeNarrative(Save, Error)) { Slot = Header; Found = true; }
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
    if (bBusy || (PendingSave && !bPendingLoadFailed)) { return ESovSaveResult::Busy; }
    if (!SovSavePolicy::ValidSlot(PolicyKind(Kind), Index)) { return ESovSaveResult::InvalidSlot; }
    if (AccountNamespace.IsEmpty()) { return ESovSaveResult::MissingAccount; }
    ASovPlayerController* PC = Controller();
    if (!PC || !PC->HasAuthority() || PC->GetWorld()->GetNetMode() != NM_Standalone)
    { Error = TEXT("Campaign load requires the standalone local controller."); return ESovSaveResult::UnsafeState; }
    TGuardValue<bool> Mutation(bBusy, true);
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
    bPendingWorldApplied = false; bPendingLoadFailed = false; PendingDestination.Reset();
    PendingLoadDeadline = FPlatformTime::Seconds() + LoadTimeoutSeconds;
    const FString Destination = PendingSave->Header.MapPackage + TEXT("?SovCampaignSlotLoad=1");
    AcknowledgeSaveFailure();
    if (!PC->GetWorld()->ServerTravel(Destination, true))
    { PendingSave = nullptr; PendingNarrative = nullptr; Error = TEXT("Saved-map travel was rejected; current world retained."); return ESovSaveResult::TravelFailed; }
    // Acceptance starts asynchronous map/managed-pawn restoration. Success notification occurs only at CharacterReady.
    return ESovSaveResult::LoadStarted;
}
void USovSaveSubsystem::ResolveInitialSave(UWorld& World, UNarrativeSave*& Snapshot, bool& bOverride)
{
    if (World.GetGameInstance() != GetGameInstance() || !PendingSave) { return; }
    const AGameModeBase* GM = World.GetAuthGameMode();
    if (!GM || !UGameplayStatics::HasOption(GM->OptionsString, TEXT("SovCampaignSlotLoad"))) { return; }
    bOverride = true; Snapshot = nullptr; PendingDestination = &World;
    FString Error;
    if (bPendingWorldApplied || !ValidatePendingWorld(World, Error)) { bPendingLoadFailed = true; return; }
    Snapshot = PendingNarrative;
    bPendingWorldApplied = true;
}
bool USovSaveSubsystem::ValidatePendingWorld(UWorld& World, FString& Error) const
{
    if (bPendingLoadFailed) { Error = TEXT("Previous campaign load failed; choose a valid recovery save."); return false; }
    if (!PendingSave) { return true; }
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
    if (!PendingSave || !PC || PC->GetWorld() != PendingDestination.Get()) { return; }
    const auto Header = PendingSave->Header;
    FString Error;
    const bool Good = bSucceeded && bPendingWorldApplied && ValidatePendingWorld(*PC->GetWorld(), Error)
        && PC->GetPawn() && Cast<ASovPlayerCharacterBase>(PC->GetPawn())
        && CastChecked<ASovPlayerCharacterBase>(PC->GetPawn())->IsCharacterReady();
    if (Good) { PlaySeconds = Header.PlaySeconds; PendingAutosaves.Reset(); }
    else { if (Error.IsEmpty()) { Error = TEXT("Campaign restoration failed; choose a compatible autosave."); } }
    PendingSave = nullptr; PendingNarrative = nullptr; PendingDestination.Reset();
    OnLoadCompleted.Broadcast(Good ? ESovSaveResult::Success : ESovSaveResult::RecoveryAvailable, Header, Error);
}
void USovSaveSubsystem::ReportSave(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Error)
{
    if (Result == ESovSaveResult::WriteFailed || Result == ESovSaveResult::ReadbackFailed)
    {
        bAwaitingFailureDecision = true; PausedController = Controller();
        if (PausedController.IsValid() && !UGameplayStatics::IsGamePaused(PausedController.Get()))
        { bOwnPause = PausedController->SetPause(true); }
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
    if (!PC || PendingSave || !AcknowledgedWorld.IsValid() || PC->GetWorld() != AcknowledgedWorld.Get()
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
        AcknowledgmentExpiresAt = FPlatformTime::Seconds() + 60.0;
    }
    if (bOwnPause && PausedController.IsValid()) { PausedController->SetPause(false); }
    bOwnPause = false; bAwaitingFailureDecision = false; PausedController.Reset();
    FailedWrite = nullptr; PendingAutosaves.Reset();
}
bool USovSaveSubsystem::Tick(float DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f || bBusy) { return true; }
    ASovPlayerController* PC = Controller();
    if (PC && PC->GetPawn() && PC->GetCampaignState()->GetActiveMission() && !UGameplayStatics::IsGamePaused(PC))
    { PlaySeconds += DeltaSeconds; }
    if (PendingSave && !bPendingLoadFailed && FPlatformTime::Seconds() > PendingLoadDeadline)
    {
        const auto Header = PendingSave->Header;
        PendingSave = nullptr; PendingNarrative = nullptr; PendingDestination.Reset(); bPendingLoadFailed = true;
        OnLoadCompleted.Broadcast(ESovSaveResult::RecoveryAvailable, Header, TEXT("Saved-map initialization timed out. Choose a last known-good autosave."));
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
