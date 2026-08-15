// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include <GameplayTagContainer.h>
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "Character/CharacterDefinition.h"
#include "NPCDefinition.generated.h"

/**
 * Data asset with information about an NPC, as well as its class. We don't store NPC information in the NPC actor itself, this is so we can access 
 * NPC information like name, icon, etc without having to load the entire NPC class into memory. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNPCDefinition : public UCharacterDefinition
{
	GENERATED_BODY()
	
public:

	UNPCDefinition();

	/** FName ID for this NPC - DialogueSpeakerIDs need this, and also sequencer needs this for linking NPCs into sequence via tag.
	 * This is deprecated, should use CharacterID instead. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "NPC")
	FName NPCID;

	/**The name of this NPC. Will be used for the interaction and navigation markers. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "NPC")
	FText NPCName;

	/** The NPCs level will not be lower than this value */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "NPC", meta = (ClampMin=1, ClampMax=5000))
	int32 MinLevel;

	/** The NPCs level will not be higher than this value */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "NPC", meta = (ClampMin=1, ClampMax=5000))
	int32 MaxLevel;

	/** Is this NPC unique, or can we spawn multiple of them? Main characters for example generally only
	want a max of one spawned in a time, whilst a generic bandit might have as many instances as we want. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "NPC")
	bool bAllowMultipleInstances;

	/** If this NPC is unique, we'll use this as the NPCs save system GUID. Since there is only one unique GUID
	defined here, this will nicely guarantee unique NPCs only ever have 1 save record created per NPC. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "NPC", meta = (EditCondition="!bAllowMultipleInstances", EditConditionHides))
	FGuid UniqueNPCGUID;

	/** Class path */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "NPC")
	TSoftClassPtr<ANarrativeNPCCharacter> NPCClassPath;

	/** The dialogue that should play when we interact with this NPC - can be left empty.  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Dialogue", meta = (AssetBundles = "SpawnedData"))
	TSoftClassPtr<class UDialogue> Dialogue;

	/** The NPCs tagged dialogues - usually free movement dialopgues that can be kicked off via a tag "TaggedDialogue.Taunt, TaggedDialogue.Greet, etc. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Dialogue", meta = (AssetBundles = "SpawnedData"))
	TSoftObjectPtr<class UTaggedDialogueSet> TaggedDialogueSet;

	/** Whether this NPCs inventory should be a vendor inventory - that is to say they are a shop vendor  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	bool bIsVendor;

	/** Default currency this character should have in their inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 TradingCurrency;

	/// The percentage of the items value we'll buy items for if we're a vendor. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	float BuyItemPercentage;

	/// The percentage of the items value we'll sell items for 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	float SellItemPercentage;

	/** The items we should grant the character by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FLootTableRoll> TradingItemLoadout;

	/** The name of the vendors shop, if this NPC is a vendor.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (EditCondition = "bIsVendor", EditConditionHides))
	FText ShopFriendlyName;

	/** The NPCs Activity schedule. */
	UPROPERTY(BlueprintReadOnly, Category = "AI", meta = (AssetBundles = "SpawnedData"))
	TArray<TSoftObjectPtr<class UNPCActivitySchedule>> ActivitySchedules;
	
	/** The NPCs activity config. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AI", meta = (AssetBundles = "SpawnedData"))
	TSoftObjectPtr<class UNPCActivityConfiguration> ActivityConfiguration;
};
