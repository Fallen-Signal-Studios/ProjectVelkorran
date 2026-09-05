// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Platform/SovPlatformServicesAdapter.h"
#include "Platform/SovPlatformServicesPolicy.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineUserCloudInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Async/Async.h"
#include "HAL/PlatformTime.h"

namespace
{
    /** One provider operation at a time. Cancelled reads/enumerations remain owned until their terminal delegate.
     * OSS callbacks contain user/file, not request IDs: dropping that ownership would let a delayed callback
     * satisfy a newer request for the same slot. There is deliberately no timeout-based unlock here. */
    class FSovOnlinePlatformServicesAdapter final : public ISovPlatformServicesAdapter,
        public TSharedFromThis<FSovOnlinePlatformServicesAdapter, ESPMode::ThreadSafe>
    {
        enum class EStage : uint8 { EnumerateRead, EnumerateWrite, EnumerateList, Read, Write, Verify, Delete };
        struct FRequest
        {
            FGuid Id;
            FUniqueNetIdPtr User;
            FString Prefix;
            FString File;
            EStage Stage = EStage::EnumerateRead;
            TArray<uint8> Upload;
            FReadComplete ReadComplete;
            FListComplete ListComplete;
            FWriteComplete WriteComplete;
            bool bCancelled = false;
            double CancelledAt = 0;
        };
    public:
        explicit FSovOnlinePlatformServicesAdapter(UGameInstance* InInstance) : Instance(InInstance) {}
        ~FSovOnlinePlatformServicesAdapter() override { Stop(); }
        void Start(FAccountChanged Changed) override
        {
            bStarted = true; const uint64 Generation = ++CallbackGeneration;
            AccountChanged = MoveTemp(Changed);
            const TWeakPtr<FSovOnlinePlatformServicesAdapter, ESPMode::ThreadSafe> Weak = AsShared();
            if (Identity)
            {
                LoginChangedHandle = Identity->AddOnLoginChangedDelegate_Handle(FOnLoginChangedDelegate::CreateLambda([Weak, Generation](int32 LocalUser)
                { Dispatch(Weak, Generation, [LocalUser](auto& Self)
                  { if ((LocalUser == Self.ResolveLocalUser() || LocalUser == INDEX_NONE) && Self.AccountChanged) { Self.AccountChanged(); } }); }));
            }
            if (Cloud)
            {
                EnumerateHandle = Cloud->AddOnEnumerateUserFilesCompleteDelegate_Handle(FOnEnumerateUserFilesCompleteDelegate::CreateLambda(
                    [Weak, Generation](bool Good, const FUniqueNetId& User)
                    { DispatchUser(Weak, Generation, User, [Good](auto& Self, const auto& Owner) { Self.Enumerated(Good, Owner); }); }));
                ReadHandle = Cloud->AddOnReadUserFileCompleteDelegate_Handle(FOnReadUserFileCompleteDelegate::CreateLambda(
                    [Weak, Generation](bool Good, const FUniqueNetId& User, const FString& File)
                    { DispatchUser(Weak, Generation, User, [Good, File](auto& Self, const auto& Owner) { Self.Read(Good, Owner, File); }); }));
                WriteHandle = Cloud->AddOnWriteUserFileCompleteDelegate_Handle(FOnWriteUserFileCompleteDelegate::CreateLambda(
                    [Weak, Generation](bool Good, const FUniqueNetId& User, const FString& File)
                    { DispatchUser(Weak, Generation, User, [Good, File](auto& Self, const auto& Owner) { Self.Written(Good, Owner, File); }); }));
                CancelHandle = Cloud->AddOnWriteUserFileCanceledDelegate_Handle(FOnWriteUserFileCanceledDelegate::CreateLambda(
                    [Weak, Generation](bool Good, const FUniqueNetId& User, const FString& File)
                    { DispatchUser(Weak, Generation, User, [Good, File](auto& Self, const auto& Owner)
                      { if (Good && Self.Matches(Owner, File, EStage::Write)) { Self.Finish(false, false, {}, TEXT("Provider cancelled upload.")); } }); }));
                DeleteHandle = Cloud->AddOnDeleteUserFileCompleteDelegate_Handle(FOnDeleteUserFileCompleteDelegate::CreateLambda(
                    [Weak, Generation](bool Good, const FUniqueNetId& User, const FString& File)
                    { DispatchUser(Weak, Generation, User, [Good, File](auto& Self, const auto& Owner)
                      { if (Self.Matches(Owner, File, EStage::Delete)) { Self.Finish(Good, false, {}, Good ? FString() : TEXT("Provider did not confirm revision deletion.")); } }); }));
            }
        }
        void Stop() override
        {
            bStarted = false; ++CallbackGeneration;
            AccountChanged = nullptr;
            if (Pending) { Pending->bCancelled = true; Pending->ReadComplete = nullptr; Pending->WriteComplete = nullptr; Pending->ListComplete = nullptr; }
            if (Identity)
            {
                if (BoundLocalUser >= 0) { Identity->ClearOnLoginStatusChangedDelegate_Handle(BoundLocalUser, LoginStatusHandle); }
                Identity->ClearOnLoginChangedDelegate_Handle(LoginChangedHandle);
            }
            if (Cloud)
            {
                Cloud->ClearOnEnumerateUserFilesCompleteDelegate_Handle(EnumerateHandle);
                Cloud->ClearOnReadUserFileCompleteDelegate_Handle(ReadHandle);
                Cloud->ClearOnWriteUserFileCompleteDelegate_Handle(WriteHandle);
                Cloud->ClearOnWriteUserFileCanceledDelegate_Handle(CancelHandle);
                Cloud->ClearOnDeleteUserFileCompleteDelegate_Handle(DeleteHandle);
            }
            Pending.Reset(); Identity.Reset(); Cloud.Reset();
            BoundSubsystem = nullptr; BoundLocalUser = INDEX_NONE; LastKnownStableId.Reset(); Provider = NAME_None;
        }
        FSovObservedPlatformAccount GetAccount() override
        {
            FSovObservedPlatformAccount Result;
            Result.bRequiresKnownStorageOwner = !PLATFORM_DESKTOP;
            Result.bUsesPlatformManagedCloud = !PLATFORM_DESKTOP;
            const int32 LocalUser = ResolveLocalUser();
            const bool ProviderReady = LocalUser >= 0 && EnsureWorldProvider();
            SynchronizeUserBinding(LocalUser);
            Result.LocalUser = LocalUser;
            Result.StableId = LastKnownStableId;
            if (LocalUser < 0) { return Result; }
            if (!ProviderReady || !Identity || Provider == FName(TEXT("NULL"))) { return Result; }
            const auto Id = Identity->GetUniquePlayerId(LocalUser);
            const auto Status = Identity->GetLoginStatus(LocalUser);
            if (Id.IsValid() && Id->IsValid())
            {
                Result.bIdentityKnown = true;
                LastKnownStableId = Provider.ToString() + TEXT("|") + Id->GetType().ToString() + TEXT("|") + Id->ToString();
                Result.bSignedIn = Status == ELoginStatus::LoggedIn;
                Result.bStorageAccessAuthorized = SovPlatformServicesPolicy::CanAuthorizeStorage(
                    Result.bRequiresKnownStorageOwner, true, Status != ELoginStatus::NotLoggedIn,
                    PLATFORM_DESKTOP || Identity->GetPlatformUserIdFromUniqueNetId(*Id) == ResolvePlatformUser(), LocalUser);
                Result.bCloudAvailable = SovPlatformServicesPolicy::CanUseGenericCloud(PLATFORM_DESKTOP,
                    Result.bSignedIn, Cloud.IsValid(), LocalUser) && GetRecoveryMessage().IsEmpty();
            }
            // Desktop offline restart retains the last confirmed namespace. On devices requiring
            // account ownership, an unknown/NotLoggedIn identity fences storage; UsingLocalProfile
            // is sufficient for offline local saves and does not require network connectivity.
            Result.StableId = LastKnownStableId;
            return Result;
        }
        bool ReadLatest(FGuid Request, const FString& Prefix, FReadComplete Complete) override
        {
            if (!Begin(Request, Prefix)) { return false; }
            Pending->Stage = EStage::EnumerateRead; Pending->ReadComplete = MoveTemp(Complete);
            const auto User = Pending->User; Cloud->EnumerateUserFiles(*User); return true;
        }
        bool WriteRevision(FGuid Request, const FString& Prefix, const TArray<uint8>& Bytes, FWriteComplete Complete) override
        {
            if (!SovPlatformServicesPolicy::CanPublishRevision(0, Bytes.Num()) || !Begin(Request, Prefix)) { return false; }
            Pending->Stage = EStage::EnumerateWrite; Pending->Upload = Bytes; Pending->WriteComplete = MoveTemp(Complete);
            const auto User = Pending->User; Cloud->EnumerateUserFiles(*User); return true;
        }
        bool SupportsRevisionHistory() const override { return true; }
        FString GetRecoveryMessage() const override
        {
            return Pending && Pending->bCancelled && Pending->CancelledAt > 0
                && FPlatformTime::Seconds() - Pending->CancelledAt >= SovPlatformServicesPolicy::OperationTimeoutSeconds
                ? TEXT("Cloud provider has not completed its cancelled request. Cloud is quarantined until the provider completes or the game restarts. Local saves remain available.")
                : FString();
        }
        bool ListRevisions(FGuid Request, const FString& Prefix, FListComplete Complete) override
        {
            if (!Begin(Request, Prefix)) { return false; }
            Pending->Stage = EStage::EnumerateList; Pending->ListComplete = MoveTemp(Complete);
            const auto User = Pending->User; Cloud->EnumerateUserFiles(*User); return true;
        }
        bool ReadRevision(FGuid Request, const FString& Prefix, const FString& Revision, FReadComplete Complete) override
        {
            if (!IsRevision(Revision, Prefix) || !Begin(Request, Prefix)) { return false; }
            Pending->Stage = EStage::Read; Pending->File = Revision; Pending->ReadComplete = MoveTemp(Complete);
            StartRead(); return true;
        }
        bool DeleteRevision(FGuid Request, const FString& Prefix, const FString& Revision, FWriteComplete Complete) override
        {
            if (!IsRevision(Revision, Prefix) || !Begin(Request, Prefix)) { return false; }
            Pending->Stage = EStage::Delete; Pending->File = Revision; Pending->WriteComplete = MoveTemp(Complete);
            const auto User = Pending->User;
            if (!Cloud->DeleteUserFile(*User, Revision, true, true) && Pending && Pending->Id == Request)
            { Finish(false, false, {}, TEXT("Provider rejected revision deletion.")); }
            return true;
        }
        void Cancel(FGuid Request) override
        {
            if (!Pending || Pending->Id != Request) { return; }
            if (!Pending->bCancelled) { Pending->CancelledAt = FPlatformTime::Seconds(); }
            Pending->bCancelled = true; Pending->ReadComplete = nullptr; Pending->WriteComplete = nullptr; Pending->ListComplete = nullptr;
            // Do not call CancelWriteUserFile: several OSS providers issue both cancellation and write
            // completion delegates. Waiting for the original terminal delegate keeps ownership exact.
            // Issued writes are immutable new revisions, so allowing them to drain cannot replace old data.
        }
    private:
        template<typename WorkType> static void Dispatch(TWeakPtr<FSovOnlinePlatformServicesAdapter, ESPMode::ThreadSafe> Weak, uint64 Generation, WorkType Work)
        {
            auto Run = [Weak, Generation, Work = MoveTemp(Work)]() mutable
            { if (const auto Self = Weak.Pin(); Self && Self->bStarted && Self->CallbackGeneration == Generation) { Work(*Self); } };
            if (IsInGameThread()) { Run(); } else { AsyncTask(ENamedThreads::GameThread, MoveTemp(Run)); }
        }
        template<typename WorkType> static void DispatchUser(TWeakPtr<FSovOnlinePlatformServicesAdapter, ESPMode::ThreadSafe> Weak, uint64 Generation,
            const FUniqueNetId& User, WorkType Work)
        {
            // Delegate references expire on the callback thread. Copy identity values, then retain
            // the matching pending provider ID while dispatching its completion on the game thread.
            const FString Key = User.GetType().ToString() + TEXT("|") + User.ToString();
            Dispatch(Weak, Generation, [Key, Work = MoveTemp(Work)](auto& Self) mutable
            {
                if (!Self.Pending || !Self.Pending->User.IsValid()) { return; }
                const auto Owner = Self.Pending->User;
                if (Owner->GetType().ToString() + TEXT("|") + Owner->ToString() == Key) { Work(Self, *Owner); }
            });
        }
        bool EnsureWorldProvider()
        {
            const UGameInstance* Game = Instance.Get();
            const UWorld* World = Game ? Game->GetWorld() : nullptr;
            if (!bStarted || !World) { return false; }
            // GI subsystems may initialize before a world, local player or platform interfaces exist.
            // Resolve against the eventual world (including PIE scope), and retry deferred interfaces.
            IOnlineSubsystem* Candidate = Online::GetSubsystem(World);
            const IOnlineIdentityPtr CandidateIdentity = Candidate ? Candidate->GetIdentityInterface() : nullptr;
            const IOnlineUserCloudPtr CandidateCloud = Candidate ? Candidate->GetUserCloudInterface() : nullptr;
            if (Candidate == BoundSubsystem && CandidateIdentity == Identity && CandidateCloud == Cloud)
            { return Identity.IsValid(); }
            // OSS terminal delegates have no request IDs. A draining request retains its exact old
            // interfaces and listeners; no replacement provider can consume or complete that request.
            if (Pending) { return false; }
            FAccountChanged Callback = MoveTemp(AccountChanged);
            Stop();
            BoundSubsystem = Candidate; Provider = Candidate ? Candidate->GetSubsystemName() : NAME_None;
            Identity = CandidateIdentity; Cloud = CandidateCloud;
            Start(MoveTemp(Callback));
            return Identity.IsValid();
        }
        FPlatformUserId ResolvePlatformUser() const
        {
            const UGameInstance* Game = Instance.Get();
            const ULocalPlayer* Player = Game ? Game->GetFirstGamePlayer() : nullptr;
            return Player ? Player->GetPlatformUserId() : PLATFORMUSERID_NONE;
        }
        int32 ResolveLocalUser() const
        {
            const FPlatformUserId User = ResolvePlatformUser();
            if (!User.IsValid()) { return INDEX_NONE; }
            return IPlatformInputDeviceMapper::Get().GetUserIndexForPlatformUser(User);
        }
        void SynchronizeUserBinding(int32 LocalUser)
        {
            if (BoundLocalUser == LocalUser) { return; }
            if (Identity && BoundLocalUser >= 0)
            { Identity->ClearOnLoginStatusChangedDelegate_Handle(BoundLocalUser, LoginStatusHandle); }
            BoundLocalUser = LocalUser;
            LastKnownStableId = PLATFORM_DESKTOP && LocalUser >= 0
                ? FString::Printf(TEXT("Offline.LocalProfile.%d"), LocalUser) : FString();
            LoginStatusHandle.Reset();
            if (Identity && LocalUser >= 0)
            {
                const TWeakPtr<FSovOnlinePlatformServicesAdapter, ESPMode::ThreadSafe> Weak = AsShared();
                const uint64 Generation = CallbackGeneration;
                LoginStatusHandle = Identity->AddOnLoginStatusChangedDelegate_Handle(LocalUser,
                    FOnLoginStatusChangedDelegate::CreateLambda([Weak, Generation](int32 ChangedUser, ELoginStatus::Type, ELoginStatus::Type, const FUniqueNetId&)
                    { Dispatch(Weak, Generation, [ChangedUser](auto& Self)
                      { if (ChangedUser == Self.BoundLocalUser && Self.AccountChanged) { Self.AccountChanged(); } }); }));
            }
        }
        bool Begin(FGuid Request, const FString& Prefix)
        {
            if (Pending || !Cloud || !Identity || !GetAccount().bCloudAvailable || !Request.IsValid()) { return false; }
            const auto User = Identity->GetUniquePlayerId(BoundLocalUser);
            if (!User.IsValid() || !User->IsValid()) { return false; }
            Pending = MakeUnique<FRequest>(); Pending->Id = Request; Pending->User = User; Pending->Prefix = Prefix;
            return true;
        }
        bool Matches(const FUniqueNetId& User, const FString& File, EStage Stage) const
        { return Pending && Pending->User.IsValid() && *Pending->User == User && Pending->File == File && Pending->Stage == Stage; }
        bool IsRevision(const FString& File, const FString& Prefix) const
        {
            if (!File.StartsWith(Prefix) || !File.EndsWith(TEXT(".sav"))) { return false; }
            const FString Suffix = File.Mid(Prefix.Len());
            FGuid Id;
            return Suffix.Len() == 19 + 1 + 32 + 4 && Suffix[19] == TCHAR('_')
                && Suffix.Left(19).IsNumeric() && FGuid::ParseExact(Suffix.Mid(20, 32), EGuidFormats::Digits, Id) && Id.IsValid();
        }
        void Enumerated(bool Good, const FUniqueNetId& User)
        {
            if (!Pending || !Pending->User.IsValid() || *Pending->User != User
                || (Pending->Stage != EStage::EnumerateRead && Pending->Stage != EStage::EnumerateWrite
                    && Pending->Stage != EStage::EnumerateList)) { return; }
            if (Pending->bCancelled) { Finish(false, false, {}, TEXT("Cancelled.")); return; }
            if (!Good) { Finish(false, false, {}, TEXT("Cloud enumeration failed; absence was not inferred from a network error.")); return; }
            TArray<FCloudFileHeader> Files; Cloud->GetUserFileList(User, Files);
            if (Pending->Stage == EStage::EnumerateList)
            {
                TArray<FSovCloudRevisionFile> Revisions;
                for (const auto& File : Files)
                {
                    if (!IsRevision(File.FileName, Pending->Prefix)) { continue; }
                    if (Revisions.Num() >= 128) { Finish(false, false, {}, TEXT("More than 128 retained revisions require provider storage recovery before review.")); return; }
                    Revisions.Add({ File.FileName, File.FileSize });
                }
                // This ordering is for stable presentation only. It never chooses a save for import.
                Revisions.Sort([](const auto& A, const auto& B) { return A.RevisionId < B.RevisionId; });
                auto Finished = MoveTemp(Pending);
                if (Finished->ListComplete) { Finished->ListComplete(true, MoveTemp(Revisions), {}); }
                return;
            }
            int32 Count = 0; FString Latest; int64 LatestSize = 0;
            for (const auto& File : Files)
            {
                if (!IsRevision(File.FileName, Pending->Prefix)) { continue; }
                ++Count;
                if (Latest.IsEmpty() || File.FileName > Latest) { Latest = File.FileName; LatestSize = File.FileSize; }
            }
            if (Pending->Stage == EStage::EnumerateWrite)
            {
                if (!SovPlatformServicesPolicy::CanPublishRevision(Count, Pending->Upload.Num()))
                { Finish(false, false, {}, TEXT("Cloud revision limit reached (32 per slot). Manage old copies using platform storage tools; local saves remain available.")); return; }
                Pending->File = Pending->Prefix + FString::Printf(TEXT("%019lld_"), static_cast<long long>(FDateTime::UtcNow().GetTicks()))
                    + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".sav");
                Pending->Stage = EStage::Write;
                // Keep the byte buffer alive across synchronous or asynchronous provider callbacks.
                const FGuid Request = Pending->Id; const auto Owner = Pending->User; const FString File = Pending->File;
                TArray<uint8> Upload = Pending->Upload;
                if (!Cloud->WriteUserFile(*Owner, File, Upload, false) && Pending && Pending->Id == Request && Pending->Stage == EStage::Write)
                { Finish(false, false, {}, TEXT("Cloud provider rejected upload. Local save retained.")); }
                return;
            }
            if (Latest.IsEmpty()) { Finish(true, false, {}, {}); return; }
            if (Count > 1) { Finish(false, true, {}, TEXT("Multiple cloud revisions require explicit history selection; client clocks cannot choose the current save.")); return; }
            if (LatestSize <= 0 || LatestSize > SovPlatformServicesPolicy::MaximumEnvelopeBytes)
            { Finish(false, true, {}, TEXT("Cloud revision has an invalid or oversized envelope. It was not imported or overwritten.")); return; }
            Pending->File = Latest; Pending->Stage = EStage::Read; StartRead();
        }
        void StartRead()
        {
            const FGuid Request = Pending->Id; const auto User = Pending->User; const FString File = Pending->File;
            // Avoid treating cached upload contents as a network readback.
            TArray<uint8> Cached;
            if (Cloud->GetFileContents(*User, File, Cached) && !Cloud->ClearFile(*User, File))
            { Finish(false, true, {}, TEXT("Provider could not clear cached file before verified cloud read.")); return; }
            if (!Cloud->ReadUserFile(*User, File) && Pending && Pending->Id == Request)
            { Finish(false, true, {}, TEXT("Cloud provider rejected read. Local saves are unchanged.")); }
        }
        void Read(bool Good, const FUniqueNetId& User, const FString& File)
        {
            if (!Matches(User, File, EStage::Read) && !Matches(User, File, EStage::Verify)) { return; }
            if (Pending->bCancelled) { Finish(false, true, {}, TEXT("Cancelled.")); return; }
            TArray<uint8> Bytes;
            if (!Good || !Cloud->GetFileContents(User, File, Bytes) || Bytes.IsEmpty() || Bytes.Num() > SovPlatformServicesPolicy::MaximumEnvelopeBytes)
            { Finish(false, true, {}, TEXT("Cloud read failed or returned an invalid envelope.")); return; }
            if (Pending->Stage == EStage::Verify && Bytes != Pending->Upload)
            { Finish(false, true, {}, TEXT("Cloud readback did not match the uploaded revision. No success was claimed.")); return; }
            Finish(true, true, MoveTemp(Bytes), {});
        }
        void Written(bool Good, const FUniqueNetId& User, const FString& File)
        {
            if (!Matches(User, File, EStage::Write)) { return; }
            if (Pending->bCancelled) { Finish(false, false, {}, TEXT("Cancelled; the provider may have retained the new immutable revision.")); return; }
            if (!Good) { Finish(false, false, {}, TEXT("Cloud upload failed. Older revisions and local save remain intact.")); return; }
            Pending->Stage = EStage::Verify; StartRead();
        }
        void Finish(bool Good, bool Exists, TArray<uint8> Bytes, FString Error)
        {
            auto Finished = MoveTemp(Pending); // Release ownership before callbacks allow another request.
            if (!Finished || Finished->bCancelled) { return; }
            if (Finished->ReadComplete) { Finished->ReadComplete(Good, Exists, MoveTemp(Bytes), MoveTemp(Error)); }
            else if (Finished->ListComplete) { Finished->ListComplete(Good, {}, MoveTemp(Error)); }
            else if (Finished->WriteComplete) { Finished->WriteComplete(Good, MoveTemp(Error)); }
        }
        FName Provider;
        IOnlineSubsystem* BoundSubsystem = nullptr;
        bool bStarted = false;
        uint64 CallbackGeneration = 0;
        TWeakObjectPtr<UGameInstance> Instance;
        FString LastKnownStableId;
        int32 BoundLocalUser = INDEX_NONE;
        IOnlineIdentityPtr Identity;
        IOnlineUserCloudPtr Cloud;
        FAccountChanged AccountChanged;
        TUniquePtr<FRequest> Pending;
        FDelegateHandle LoginStatusHandle, LoginChangedHandle, EnumerateHandle, ReadHandle, WriteHandle, CancelHandle, DeleteHandle;
    };
}
TSharedPtr<ISovPlatformServicesAdapter, ESPMode::ThreadSafe> MakeSovConfiguredPlatformAdapter(UGameInstance* Instance)
{ return MakeShared<FSovOnlinePlatformServicesAdapter, ESPMode::ThreadSafe>(Instance); }
