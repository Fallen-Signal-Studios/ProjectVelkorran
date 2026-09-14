// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UAssetManager;

/** A package that enters a cook without any mission referencing it, with the configured route that adds it. */
struct FSovCookRoot
{
    FName Package;
    FString Route;
};

/** Configured cook entry points other than Asset Manager rules. Read from config by ReadConfiguredCookInputs,
 * or supplied directly so the gathering rules can be exercised without changing project settings. */
struct FSovConfiguredCookInputs
{
    /** GameMapsSettings keys that UE 5.7 GetGameDefaultObjects cooks, paired with their object paths. */
    TArray<TPair<FString, FString>> GameDefaults;
    /** Packaging MapsToCook and the editor AlwaysCookMaps list, as package or object paths. */
    TArray<FString> Maps;
    /** Packaging DirectoriesToAlwaysCook, as long package paths. */
    TArray<FString> Directories;
    /** InputSettings DefaultTouchInterface, which CollectFilesToCook adds unless empty or None. */
    FString TouchInterface;
    /** Native config classes. Their default objects' config references are what the cooker collects at
     * startup: soft paths become StartupSoftObjectPath roots, loaded classes become startup packages. */
    TArray<const UClass*> ConfigClasses;
};

namespace SovCampaignContentValidation
{
/** Parse a comma-separated asset list without truncating at the first comma.
 * Match is the option name including '='. Quoted values remain supported; whitespace ends unquoted values. */
PROJECTVELKORRAN_API bool ParseAssetListArgument(const FString& Params, const TCHAR* Match, TArray<FString>& OutPaths);

/** Narrow campaign exclusions, not a ban on Narrative's shared item/GAS/UI infrastructure.
 * Empty means no known prohibited system was identified, not arbitrary Blueprint certification. */
PROJECTVELKORRAN_API FString ProhibitedAssetReason(const UObject* Asset);

/** The authored-name plus class rule table behind ProhibitedAssetReason.
 * Exposed so the rules can be exercised without fabricating assets: several prohibited
 * systems are widgets, and UUserWidget is abstract and cannot be instantiated directly.
 * Takes the authored name with any "_C" suffix already removed. */
PROJECTVELKORRAN_API FString ProhibitedSystemReasonForName(const FString& AuthoredName, const UClass* EffectiveClass);

/** Narrative Pro demo/template items granted by a campaign character's default loadout.
 * Aurelion roles must carry authored project equipment or rely on their ability kit; a demo
 * placeholder reaching a campaign actor is a content defect, not a framework fault. Soft paths
 * are inspected without loading, so demo content is never pulled into the validation process.
 * Empty means no demo item grant was found. */
PROJECTVELKORRAN_API FString DemoItemLoadoutReason(const UObject* Asset);

/** Narrative Pro demo tale content: a demo quest or dialogue reached by campaign content.
 * TDD Appendix F forbids vendor systems, and Narrative's demo root carries a SecretMerchant quest
 * and dialogue. A path match alone is deliberately not the rule: demo VFX, audio and meshes are
 * legitimately reused, so the asset must also be a quest or dialogue. Empty means no demo tale
 * content was identified. */
PROJECTVELKORRAN_API FString DemoTaleContentReason(const UObject* Asset);

/** Read effective production AlwaysCook rules, including bundle packages, without changing cook settings.
 * The existing dependency walker must traverse these roots as well as explicit mission roots. */
PROJECTVELKORRAN_API bool GatherAlwaysCookPackages(UAssetManager& Manager, TArray<FName>& OutPackages, FString& Error);

/** Read the current editor process's configured cook inputs for the host platform. */
PROJECTVELKORRAN_API FSovConfiguredCookInputs ReadConfiguredCookInputs();

/** Every package the configured inputs place in a cook, deduplicated with the first route that adds it.
 * Mirrors UE 5.7: soft references on editor-only config properties and properties marked Untracked are not
 * collected, native /Script packages are not content, and ServerDefaultMap is excluded because a default cook
 * does not include server maps. Directories expand through the asset registry. */
PROJECTVELKORRAN_API void GatherConfiguredCookRoots(const FSovConfiguredCookInputs& Inputs, TArray<FSovCookRoot>& OutRoots);

/** The cooked packages named by a `-run=cook -CookList` log. Rejected packages are excluded: they were discovered
 * but do not ship. False when the text names no cooked package. */
PROJECTVELKORRAN_API bool ReadCookListRoots(const FString& CookListText, TArray<FName>& OutPackages);

/** Parent map uses NAME_None for roots. Bounded even if diagnostic input contains a cycle. */
PROJECTVELKORRAN_API FString DescribeDependencyChain(FName Package, const TMap<FName, FName>& Parents);
}
