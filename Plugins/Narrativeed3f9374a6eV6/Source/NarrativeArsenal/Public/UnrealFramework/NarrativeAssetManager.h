// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include <GameplayTagContainer.h>
#include "NarrativeAssetManager.generated.h"

/**
 * Custom asset manager for Narrative pro. Used for GAS and efficient NPC caching. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeAssetManager : public UAssetManager
{
	GENERATED_BODY()
	

public:

	static UNarrativeAssetManager& Get();

	// Returns the asset referenced by a TSoftObjectPtr.  This will synchronously load the asset if it's not already loaded.
	template<typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	// Returns the subclass referenced by a TSoftClassPtr.  This will synchronously load the asset if it's not already loaded.
	template<typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/** Starts initial load, gets called from InitializeObjectReferences */
	virtual void StartInitialLoading() override;

	// Thread safe way of adding a loaded asset to keep in memory.
	void AddLoadedAsset(const UObject* Asset);

	void InitializeGAS();


};
