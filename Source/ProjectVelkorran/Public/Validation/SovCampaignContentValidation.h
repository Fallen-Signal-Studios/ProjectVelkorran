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

/** Read effective production AlwaysCook rules, including bundle packages, without changing cook settings.
 * The existing dependency walker must traverse these roots as well as explicit mission roots. */
PROJECTVELKORRAN_API bool GatherAlwaysCookPackages(UAssetManager& Manager, TArray<FName>& OutPackages, FString& Error);

/** Parent map uses NAME_None for roots. Bounded even if diagnostic input contains a cycle. */
PROJECTVELKORRAN_API FString DescribeDependencyChain(FName Package, const TMap<FName, FName>& Parents);
}
