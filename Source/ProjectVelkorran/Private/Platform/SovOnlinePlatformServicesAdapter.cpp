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

namespace
{
    /** One provider operation at a time. Cancelled reads/enumerations remain owned until their terminal delegate.
     * OSS callbacks contain user/file, not request IDs: dropping that ownership would let a delayed callback
     * satisfy a newer request for the same slot. There is deliberately no timeout-based unlock here. */
    class FSovOnlinePlatformServicesAdapter final : public ISovPlatformServicesAdapter,
        public TSharedFromThis<FSovOnlinePlatformServicesAdapter>
    {
        enum class EStage : uint8 { EnumerateRead, EnumerateWrite, Read, Write, Verify };
        struct FRequest
        {
            FGuid Id;
            FUniqueNetIdPtr User;
            FString Prefix;
            FString File;
            EStage Stage = EStage::EnumerateRead;
            TArray<uint8> Upload;
            FReadComplete ReadComplete;
            FWriteComplete WriteComplete;
            bool bCancelled = false;
        };
    public:
        explicit FSovOnlinePlatformServicesAdapter(UGameInstance* InInstance) : Instance(InInstance) {}
        ~FSovOnlinePlatformServicesAdapter() override { Stop(); }
        void Start(FAccountChanged Changed) override
        {
            bStarted = true;
            AccountChanged = MoveTemp(Changed);
            const TWeakPtr<FSovOnlinePlatformServicesAdapter> Weak = AsShared();
            if (Identity)
            {
                LoginChangedHandle = Identity->AddOnLoginChangedDelegate_Handle(FOnLoginChangedDelegate::CreateLambda([Weak](int32 LocalUser)
                    { if (!IsInGameThread()) { return; } if (const auto Self = Weak.Pin(); Self && (LocalUser == Self->ResolveLocalUser() || LocalUser == INDEX_NONE) && Self->AccountChanged) { Self->AccountChanged(); } }));
            }
            if (Cloud)
            {
                EnumerateHandle = Cloud->AddOnEnumerateUserFilesCompleteDelegate_Handle(FOnEnumerateUserFilesCompleteDelegate::CreateLambda(
                    [Weak](bool Good, const FUniqueNetId& User) { if (!IsInGameThread()) { return; } if (auto Self = Weak.Pin()) { Self->Enumerated(Good, User); } }));
                ReadHandle = Cloud->AddOnReadUserFileCompleteDelegate_Handle(FOnReadUserFileCompleteDelegate::CreateLambda(
                    [Weak](bool Good, const FUniqueNetId& User, const FString& File) { if (!IsInGameThread()) { return; } if (auto Self = Weak.Pin()) { Self->Read(Good, User, File); } }));
                WriteHandle = Cloud->AddOnWriteUserFileCompleteDelegate_Handle(FOnWriteUserFileCompleteDelegate::CreateLambda(
                    [Weak](bool Good, const FUniqueNetId& User, const FString& File) { if (!IsInGameThread()) { return; } if (auto Self = Weak.Pin()) { Self->Written(Good, User, File); } }));
                CancelHandle = Cloud->AddOnWriteUserFileCanceledDelegate_Handle(FOnWriteUserFileCanceledDelegate::CreateLambda(
                    [Weak](bool Good, const FUniqueNetId& User, const FString& File)
                    { if (!IsInGameThread()) { return; } if (auto Self = Weak.Pin(); Self && Good && Self->Matches(User, File, EStage::Write)) { Self->Finish(false, false, {}, TEXT("Provider cancelled upload.")); } }));
            }
        }
        void Stop() override
        {
            bStarted = false;
            AccountChanged = nullptr;
            if (Pending) { Pending->bCancelled = true; Pending->ReadComplete = nullptr; Pending->WriteComplete = nullptr; }
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
                    Result.bSignedIn, Cloud.IsValid(), LocalUser);
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
        void Cancel(FGuid Request) override
        {
            if (!Pending || Pending->Id != Request) { return; }
            Pending->bCancelled = true; Pending->ReadComplete = nullptr; Pending->WriteComplete = nullptr;
            // Do not call CancelWriteUserFile: several OSS providers issue both cancellation and write
            // completion delegates. Waiting for the original terminal delegate keeps ownership exact.
            // Issued writes are immutable new revisions, so allowing them to drain cannot replace old data.
        }
    private:
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
                const TWeakPtr<FSovOnlinePlatformServicesAdapter> Weak = AsShared();
                LoginStatusHandle = Identity->AddOnLoginStatusChangedDelegate_Handle(LocalUser,
                    FOnLoginStatusChangedDelegate::CreateLambda([Weak](int32 ChangedUser, ELoginStatus::Type, ELoginStatus::Type, const FUniqueNetId&)
                    { if (!IsInGameThread()) { return; } if (const auto Self = Weak.Pin(); Self && ChangedUser == Self->BoundLocalUser && Self->AccountChanged) { Self->AccountChanged(); } }));
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
                || (Pending->Stage != EStage::EnumerateRead && Pending->Stage != EStage::EnumerateWrite)) { return; }
            if (Pending->bCancelled) { Finish(false, false, {}, TEXT("Cancelled.")); return; }
            if (!Good) { Finish(false, false, {}, TEXT("Cloud enumeration failed; absence was not inferred from a network error.")); return; }
            TArray<FCloudFileHeader> Files; Cloud->GetUserFileList(User, Files);
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
            else if (Finished->WriteComplete) { Finished->WriteComplete(Good, MoveTemp(Error)); }
        }
        FName Provider;
        IOnlineSubsystem* BoundSubsystem = nullptr;
        bool bStarted = false;
        TWeakObjectPtr<UGameInstance> Instance;
        FString LastKnownStableId;
        int32 BoundLocalUser = INDEX_NONE;
        IOnlineIdentityPtr Identity;
        IOnlineUserCloudPtr Cloud;
        FAccountChanged AccountChanged;
        TUniquePtr<FRequest> Pending;
        FDelegateHandle LoginStatusHandle, LoginChangedHandle, EnumerateHandle, ReadHandle, WriteHandle, CancelHandle;
    };
}
TSharedPtr<ISovPlatformServicesAdapter> MakeSovConfiguredPlatformAdapter(UGameInstance* Instance)
{ return MakeShared<FSovOnlinePlatformServicesAdapter>(Instance); }
