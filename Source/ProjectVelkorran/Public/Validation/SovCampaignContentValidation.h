// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UAssetManager;

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

/** Read effective production AlwaysCook rules, including bundle packages, without changing cook settings.
 * The existing dependency walker must traverse these roots as well as explicit mission roots. */
PROJECTVELKORRAN_API bool GatherAlwaysCookPackages(UAssetManager& Manager, TArray<FName>& OutPackages, FString& Error);

/** Parent map uses NAME_None for roots. Bounded even if diagnostic input contains a cycle. */
PROJECTVELKORRAN_API FString DescribeDependencyChain(FName Package, const TMap<FName, FName>& Parents);
}
