// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/InventoryComponent.h"
#include <GameplayTagContainer.h>
#include "CharacterDefinition.generated.h"

/**
 * Data asset containing character data - the characters default appearance, etc.
 */
UCLASS()
class NARRATIVEARSENAL_API UCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	UCharacterDefinition();

	/** FName ID for this character - DialogueSpeakerIDs need this, and also sequencer needs this for linking chars into sequence via tag.*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character")
	FName CharacterID;
	
	/**The characters default appearance*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Appearance", meta = (AssetBundles = "SpawnedData"))
	TSoftObjectPtr<class UCharacterAppearanceBase> DefaultAppearance; 

	/** Default currency this character should have in their inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 DefaultCurrency;

	/** The items we should grant the character by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FLootTableRoll> DefaultItemLoadout;

	/** Add any custom tags the character needs in here, for example State.Invulnerable if you want the character to never take damage. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character", meta = (Categories = "Narrative.State"))
	FGameplayTagContainer DefaultOwnedTags;

	/**The factions this character will be in by default - you can set the characters faction to something else later if desired. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Factions", meta = (Categories = "Narrative.Factions"))
	FGameplayTagContainer DefaultFactions;

	/** The triggers that should exist on this character by default.  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Triggers", meta = (AssetBundles = "SpawnedData"))
	TArray<TSoftObjectPtr<class UTriggerSet>> TriggerSets;

	/** Bots use this in their EQS query when looking for targets. Higher values make bots more drawn to us. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attack Priority")
	float AttackPriority;

	// Contains some default abilities to grant, attributes, etc. 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Abilities")
	TObjectPtr<class UAbilityConfiguration> AbilityConfiguration;

    UPROPERTY()
    FPrimaryAssetType AssetType;

	FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(AssetType, GetFName());
	}
};
