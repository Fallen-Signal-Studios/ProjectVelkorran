// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Engine/DataAsset.h"
#include "Tales/NarrativeEvent.h"
#include "SovCampaignContentValidationFixtures.generated.h"

// The known legacy name exists only in this Editor test module. These reflected classes
// exercise real UClass inheritance without manufacturing an unlinked generated class.
UCLASS(Transient, NotBlueprintable)
class UNE_GiveXP : public UNarrativeEvent
{
    GENERATED_BODY()
};

UCLASS(Transient, NotBlueprintable)
class USovRenamedCampaignRewardTestEvent : public UNE_GiveXP
{
    GENERATED_BODY()
};

/** Concrete storage asset: UDataAsset and UPrimaryDataAsset are both abstract in UE 5.7. */
UCLASS(Transient, NotBlueprintable)
class USovCampaignContentValidationDataAsset : public UDataAsset
{
    GENERATED_BODY()
};

/** Isolated rule provider; never changes the editor's singleton AssetManager or config. */
UCLASS(Transient, NotBlueprintable)
class USovCampaignContentValidationAssetManager : public UAssetManager
{
    GENERATED_BODY()
public:
    // Avoid registering a test rule provider with global editor/AssetRegistry delegates.
    virtual void PostInitProperties() override { UObject::PostInitProperties(); }
    TArray<FPrimaryAssetTypeInfo> Types;
    TMap<FPrimaryAssetId, FAssetData> Assets;
    TMap<FPrimaryAssetId, TArray<FAssetBundleEntry>> Bundles;
    TMap<FName, EPrimaryAssetCookRule> PackageRules;
    TSet<FName> NonCookable;
    bool bDatabaseUpdated = false;

    virtual void UpdateManagementDatabase(EUpdateManagementDatabaseFlags InFlags = EUpdateManagementDatabaseFlags::BuildChunkMap) override
    { bDatabaseUpdated = true; }
    virtual void GetPrimaryAssetTypeInfoList(TArray<FPrimaryAssetTypeInfo>& OutTypes) const override { OutTypes = Types; }
    virtual bool GetPrimaryAssetIdList(FPrimaryAssetType Type, TArray<FPrimaryAssetId>& OutIds,
        EAssetManagerFilter Filter = EAssetManagerFilter::Default) const override
    {
        OutIds.Reset();
        for (const auto& Pair : Assets) { if (Pair.Key.PrimaryAssetType == Type) { OutIds.Add(Pair.Key); } }
        return !OutIds.IsEmpty();
    }
    virtual bool GetPrimaryAssetData(const FPrimaryAssetId& Id, FAssetData& Data) const override
    { if (const FAssetData* Found = Assets.Find(Id)) { Data = *Found; return true; } return false; }
    virtual bool GetAssetBundleEntries(const FPrimaryAssetId& Id, TArray<FAssetBundleEntry>& OutEntries) const override
    { if (const auto* Found = Bundles.Find(Id)) { OutEntries = *Found; return true; } return false; }
    virtual EPrimaryAssetCookRule GetPackageCookRule(FName Package) const override
    { const auto* Found = PackageRules.Find(Package); return Found ? *Found : EPrimaryAssetCookRule::Unknown; }
    virtual bool VerifyCanCookPackage(UE::Cook::ICookInfo* CookInfo, FName Package, bool bLogError = true) const override
    { return !NonCookable.Contains(Package); }
};
