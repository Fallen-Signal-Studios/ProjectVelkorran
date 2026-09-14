// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "SovCampaignCookRootValidationFixtures.generated.h"

/** A native config class shaped like the settings the cooker reads at startup. It has no config section; tests
 * set its default object directly and restore it, so no project setting changes. */
UCLASS(Transient, NotBlueprintable, config = Game)
class USovCookRootConfigFixture : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(config) FSoftObjectPath EntryMap;
    UPROPERTY(config) TSoftObjectPtr<UObject> SoftAsset;
    UPROPERTY(config) TSoftClassPtr<UObject> SoftClass;
    /** Config may hold class references, not object instances; a loaded class is a startup package. */
    UPROPERTY(config) TSubclassOf<UObject> LoadedClass;
    UPROPERTY(config) TSubclassOf<UObject> NativeClass;
    UPROPERTY(config) TArray<FSoftObjectPath> MapList;
    UPROPERTY(config, meta = (Untracked)) FSoftObjectPath UntrackedPath;
    UPROPERTY() FSoftObjectPath NotConfigPath;
#if WITH_EDITORONLY_DATA
    UPROPERTY(config) FSoftObjectPath EditorOnlyPath;
#endif
};
