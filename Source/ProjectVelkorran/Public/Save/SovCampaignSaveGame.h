// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "SovCampaignSaveGame.generated.h"

UENUM(BlueprintType)
enum class ESovSaveSlotKind : uint8 { Manual, Auto, Checkpoint };
UENUM(BlueprintType)
enum class ESovSaveBoundary : uint8
{
    MissionStart, ArenaEntry, ArenaExit, BeforeChoice, CanonGate, BossRetry,
    LongTransition, HubExit, ExplicitCheckpoint
};
UENUM(BlueprintType)
enum class ESovSaveResult : uint8
{
    Success, LoadStarted, Busy, UnsafeState, InvalidSlot, MissingAccount, MissingSave, CorruptSave,
    IncompatibleSave, MissingRequiredAsset, CaptureFailed, WriteFailed, ReadbackFailed,
    TravelFailed, RecoveryAvailable, AwaitingFailureDecision
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovSaveSlotHeader
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Product = TEXT("SovereignCall.Origins.Campaign");
    UPROPERTY(BlueprintReadOnly) int32 SchemaMajor = 1;
    UPROPERTY(BlueprintReadOnly) int32 SchemaMinor = 0;
    UPROPERTY(BlueprintReadOnly) FString Build;
    /** Opaque account hash. No display name, email or network identifier is written. */
    UPROPERTY(BlueprintReadOnly) FString AccountNamespace;
    UPROPERTY(BlueprintReadOnly) ESovSaveSlotKind Kind = ESovSaveSlotKind::Manual;
    UPROPERTY(BlueprintReadOnly) int32 SlotIndex = 0;
    UPROPERTY(BlueprintReadOnly) int64 Generation = 0;
    UPROPERTY(BlueprintReadOnly) FDateTime TimestampUtc;
    UPROPERTY(BlueprintReadOnly) double PlaySeconds = 0;
    UPROPERTY(BlueprintReadOnly) FName MissionId;
    UPROPERTY(BlueprintReadOnly) FGameplayTag ActiveProtagonist;
    UPROPERTY(BlueprintReadOnly) FText MissionLabel;
    UPROPERTY(BlueprintReadOnly) FName BoundaryId;
    UPROPERTY(BlueprintReadOnly) ESovSaveBoundary BoundaryKind = ESovSaveBoundary::ExplicitCheckpoint;
    UPROPERTY(BlueprintReadOnly) FString MapPackage;
    UPROPERTY(BlueprintReadOnly) FName Difficulty = TEXT("Standard");
    UPROPERTY(BlueprintReadOnly) FSoftObjectPath MissionDefinition;
    UPROPERTY(BlueprintReadOnly) TArray<FString> MigrationHistory;
};

/** Versioned envelope of the existing configured UNarrativeSave subclass, never another actor snapshot. */
UCLASS()
class PROJECTVELKORRAN_API USovCampaignSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() FSovSaveSlotHeader Header;
    UPROPERTY() TArray<uint8> NarrativePayload;
    UPROPERTY() TArray<FSoftObjectPath> RequiredAssets;
    UPROPERTY() TArray<uint8> PortableSettings;
    UPROPERTY() uint32 IntegrityChecksum = 0;
    UPROPERTY(Transient) bool bReadLegacyUnframed = false;
    uint32 CalculateChecksum() const;
    bool HasValidIntegrity() const;
    /** Framed raw bytes are checked before UObject construction. Legacy current-product banks are read, never rewritten in place. */
    static bool EncodeFramed(USovCampaignSaveGame* Save, TArray<uint8>& Bytes, FString& Error);
    static USovCampaignSaveGame* DecodeFramed(const TArray<uint8>& Bytes, FString& Error, bool* bLegacy = nullptr);
    /** Decode into the already configured class rather than trusting a class path supplied by a file. */
    static class USaveGame* DecodeKnownSave(const TArray<uint8>& Bytes, UClass* ExpectedClass, UObject* Outer, FString& Error);
};
