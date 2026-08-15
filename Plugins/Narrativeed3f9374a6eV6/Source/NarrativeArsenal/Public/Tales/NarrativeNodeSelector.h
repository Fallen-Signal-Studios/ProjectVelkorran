// Copyright Narrative Tools 2025.

#pragma once

#include "NarrativeNodeSelector.generated.h"

class UQuest;
class UDialogue;

USTRUCT(BlueprintType, meta=(HiddenByDefault, BlueprintInternalUseOnly="true"))
struct NARRATIVEARSENAL_API FNodeIDSelector
{
	GENERATED_BODY()
	
	// actual node id
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NodeID")
	FName NodeID;
	
};

// selects dialogue nodes
USTRUCT(BlueprintType, meta=(HasNativeMake="/Script/NarrativePro.NarrativeFunctionLibrary.MakeDialogueNodeSelector", HasNativeBreak="/Script/NarrativePro.NarrativeFunctionLibrary.BreakDialogueNodeSelector"))
struct NARRATIVEARSENAL_API FDialogueNodeSelector final : public FNodeIDSelector
{
	GENERATED_BODY()

	// dialogue asset 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NodeID")
	TSoftClassPtr<UDialogue> Asset;
	
};

// base type for quest node selectors
USTRUCT(NotBlueprintType, meta=(HiddenByDefault, BlueprintInternalUseOnly="true"))
struct FQuestNodeSelector : public FNodeIDSelector
{
	GENERATED_BODY()
	
	// quest asset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NodeID")
	TSoftClassPtr<UQuest> Asset;
	
};

// selects quest state nodes
USTRUCT(BlueprintType, meta=(HasNativeMake="/Script/NarrativePro.NarrativeFunctionLibrary.MakeQuestStateSelector", HasNativeBreak="/Script/NarrativePro.NarrativeFunctionLibrary.BreakQuestStateSelector"))
struct NARRATIVEARSENAL_API FQuestStateSelector final : public FQuestNodeSelector
{
	GENERATED_BODY()
};

// selects quest branch nodes
USTRUCT(BlueprintType, meta=(HasNativeMake="/Script/NarrativePro.NarrativeFunctionLibrary.MakeQuestNodeSelector", HasNativeBreak="/Script/NarrativePro.NarrativeFunctionLibrary.BreakQuestNodeSelector"))
struct NARRATIVEARSENAL_API FQuestBranchSelector final : public FQuestNodeSelector
{
	GENERATED_BODY()
};

