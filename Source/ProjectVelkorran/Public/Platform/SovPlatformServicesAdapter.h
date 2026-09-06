// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
class UGameInstance;

struct PROJECTVELKORRAN_API FSovObservedPlatformAccount
{
    // Raw provider ID is transient only; never put this value in logs, config or save headers.
    FString StableId = TEXT("Offline.LocalProfile.0");
    int32 LocalUser = 0;
    bool bSignedIn = false;
    /** False means unknown network identity, not proof that the OS/local save owner changed. */
    bool bIdentityKnown = false;
    bool bCloudAvailable = false;
    /** Console/device profiles need positive local account ownership, including offline UsingLocalProfile. */
    bool bRequiresKnownStorageOwner = false;
    bool bStorageAccessAuthorized = false;
    /** The generic immutable-file transport is not a console platform save synchronization adapter. */
    bool bUsesPlatformManagedCloud = false;
};
/** Production uses the configured IOnlineIdentity/IOnlineUserCloud, not a filesystem pretending to be cloud. */
class PROJECTVELKORRAN_API ISovPlatformServicesAdapter
{
public:
    using FAccountChanged = TFunction<void()>;
    using FReadComplete = TFunction<void(bool, bool, TArray<uint8>, FString)>;
    using FWriteComplete = TFunction<void(bool, FString)>;
    virtual ~ISovPlatformServicesAdapter() = default;
    virtual void Start(FAccountChanged Changed) = 0;
    virtual void Stop() = 0;
    virtual FSovObservedPlatformAccount GetAccount() = 0;
    virtual bool ReadLatest(FGuid Request, const FString& Prefix, FReadComplete Complete) = 0;
    virtual bool WriteRevision(FGuid Request, const FString& Prefix, const TArray<uint8>& Bytes, FWriteComplete Complete) = 0;
    /** Cancel consumption, not a promise that the provider can undo an already-issued request. */
    virtual void Cancel(FGuid Request) = 0;
};
PROJECTVELKORRAN_API TSharedPtr<ISovPlatformServicesAdapter> MakeSovConfiguredPlatformAdapter(UGameInstance* Instance);
