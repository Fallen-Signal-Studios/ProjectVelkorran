// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TaggedDialogueSet.generated.h"

/**
 * Contains various dialogues to play when a NPC greets, threatens, investigates, and so on. 
 */
UCLASS()
class NARRATIVEARSENAL_API UTaggedDialogueSet : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UTaggedDialogueSet(const FObjectInitializer& ObjectInitializer);

	/** The NPCs tagged dialogues these are essentially dialogue, usually free movement that can be kicked off via a tag "TaggedDialogue.Taunt, TaggedDialogue.Greet, etc. */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Dialogue")
	TArray<FTaggedDialogue> TaggedDialogues;

};
