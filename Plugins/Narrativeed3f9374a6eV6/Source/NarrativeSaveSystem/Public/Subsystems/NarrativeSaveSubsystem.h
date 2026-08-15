// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NarrativeSave.h"
#include "NarrativeSaveSubsystem.generated.h"

//Called when the save system updates 
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSavePhaseChanged);

//Called when a stable actor is spawned in
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStableActorSpawned, class AActor*, Actor, const FGuid, StableGUID);

/**
 * Excellent little subsystem for saving every actor in our world implementing INarrativeSavableActor, along with any INarrativeSavableComponents the actor has. 
 * By default the actors transform will be remembered, along with any UPROPERTY(SaveGame) values on the actor/saved components itself. 
 * Also contains logic for grabbing the arguments passed into our game mode's URL so that the main menu can nicely feed the save game to load and we can load it
 * before all actors in the world call BeginPlay(). 
 * 
 * Supports dynamically spawned actors, and will also handle saving and loading actors that are streamed/unstreamed via world partition. If you need to save an actor, 
 * just give it the INarrativeSavableActor interface, and implement the Load function. 
 */
UCLASS()
class NARRATIVESAVESYSTEM_API UNarrativeSaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:

	UNarrativeSaveSubsystem();

	//Create/update the save game object, and store the worlds state in it. This won't actually save it to disk. 
	virtual bool UpdateSaveObject(const bool bSkipRecordCreation=false);
	
	/**
	* Will write to the records to a save file, and actually commit the save file to disk also. 
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool Save(const FString& SaveName = "NarrativeSave", const int32 Slot = 0);

	/**
	* Assuming a save file exists, this will load the save file in. Essentially this will update the state of any actors in the world implementing INarrativeSavableActor
	* and spawn in any missing actors in the save file that aren't in the world already. We will also check for any components implemeting INarrativeSavableComponent 
	* and save their state too. 
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool Load(const FString& SaveName = "NarrativeSave", const int32 Slot = 0);

	/**Deletes a saved game from disk. USE THIS WITH CAUTION. Return true if save file deleted, false if delete failed or file didn't exist.*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool DeleteSave(const FString& SaveName = "NarrativeSave", const int32 Slot = 0);

	virtual void LoadPlayerData();

	/*Player-only saves are saves intended for networked games that just save a single players data to its own save file, skipping the world data. This allow for useful stuff like
	persistence, where we can change levels and keep each players items and weapons in its own file. We can also support having players leave and re-join the game later,
	whilst keeping all their stuff saved. */
	virtual bool CreatePlayerOnlySave(APlayerController* PC);
	virtual bool LoadPlayerOnlySave(APlayerController* PC);
	virtual bool DeletePlayerOnlySave(APlayerController* PC);

	/**Saves as usual, but intended for a networked game. Will also create player-only saves for every player currently in the server. */
	virtual bool MultiplayerSave(const FString& SaveName="ServerWorld");

	/** Load as usual, but intended for a networked game. Will also load player-only saves for every player currently in the server.
	 * @param bChangeMap if true, we'll load whatever map was saved into the servers save file. This defaults to true and is typically what you'd want. 
	 */
	virtual bool MultiplayerLoad(const FString& SaveName="ServerWorld", const bool bChangeMap=true);
	
	UFUNCTION()
	void DeferredLoadPlayerData();

	/** Update a single actor so its state matches that of the saved record for that actor. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool LoadSingleActor(class AActor* Actor);
	
	/** Update a single actor so its state matches that of the saved record for that actor. 
	
	@param bDontRespawn tells the save system that this actor should not automatically be respawned by the save system, we need to do it manually.*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool SaveSingleActor(class AActor* Actor);

	/* Remove a single actor from the save file */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool RemoveSingleActor(class AActor* Actor);

	/**Check if a record exists with the given ID. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Saving")
	bool DoesRecordExist(const FGuid& RecordGUID) const;


	virtual bool LoadDynamicRecord(const FGuid& RecordGUID);
	virtual bool LoadDynamicRecord(const FNarrativeActorRecord& DynamicRecord);

	/** Whether or not to disable saving. Useful for certain times - perhaps you want to disable saving whilst we are in combat etc. */
	UFUNCTION(BlueprintCallable, Category = "Saving")
	void SetSavingDisabled(const bool bShoudldDisable);

	/** Whether or not to disable saving. Useful for certain times - perhaps you want to disable saving whilst we are in combat etc. */
	UFUNCTION(BlueprintCallable, Category = "Saving")
	bool IsSavingDisabled() const { return bSavingDisabled; };

	/** Return true if we're a new game, ie we've started a new game and havent saved yet  */
	UFUNCTION(BlueprintPure, Category = "Saving")
	bool IsNewGame() const;

	/** Whether or not we have player data and we're just waiting to do a deferred load of it */
	UFUNCTION(BlueprintPure, Category = "Saving")
	bool IsLoading() const;

	/** Return our current save object if one exists  */
	UFUNCTION(BlueprintPure, Category = "Saving")
	FORCEINLINE class UNarrativeSave* GetSaveObject() const {return NarrativeSaveGame; };

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "Delegates")
	FOnStableActorSpawned OnStableActorSpawned;

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "Delegates")
	FOnStableActorSpawned OnStableActorDestroyed;

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "Delegates")
	FOnSavePhaseChanged OnBeginLoad;

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "Delegates")
	FOnSavePhaseChanged OnFinishedLoad;

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "Delegates")
	FOnSavePhaseChanged OnBeginSave;

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "Delegates")
	FOnSavePhaseChanged OnFinishedSave;

	/** Allows you to quickly lookup an actor reference using its save GUID. Useful for actor references - save the GUID to disk and look it up later.  */
	UFUNCTION(BlueprintPure, Category = "Lookups")
	AActor* LookupActorByGUID(const FGuid& SearchGUID);

	//Helper functions for creating a record from an actor, or initializing an actor from an actor record. 
	bool CreateActorRecord(class AActor* Actor, FNarrativeActorRecord& ActorRecord) const;
	void LoadActorFromRecord(class AActor* Actor, const FNarrativeActorRecord& ActorRecord) const;

	/** Called by GameMode->InitGame */
	void InitializeSaveSystem(UWorld& InWorld);

protected:

	UPROPERTY()
	TObjectPtr<class UNarrativeSave> NarrativeSaveGame;

	FString OptionString;

	//Whether or not we're currnetly loading data in. 
	UPROPERTY(BlueprintReadOnly, Category = "Save Info")
	bool bIsCurrentlyLoading;

	//The save slot we're currently playing in, whether we've loaded it or just saved to this slot. 
	UPROPERTY(BlueprintReadOnly, Category = "Save Info")
	int32 CurrentSaveSlot;

	//The save name we're currently playing in, whether we've loaded it or just saved to this slot. 
	UPROPERTY(BlueprintReadOnly, Category = "Save Info")
	FString CurrentSaveName;

	virtual void PostInitialize() override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	FDelegateHandle ActorPrespawnedHandle;
	FDelegateHandle ActorSpawnedHandle;
	FDelegateHandle ActorDestroyedHandle;

	UFUNCTION()
	void OnActorPrespawned(class AActor* SpawnedActor);

	UFUNCTION()
	void OnActorSpawned(class AActor* SpawnedActor);

	UFUNCTION()
	void OnActorDestroyed(class AActor* DestroyedActor);

	UFUNCTION()
	void PreLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);

	UFUNCTION()
	void LevelAddedToWorld(ULevel* InLevel, UWorld* InWorld);
	
	//We want these so we can quitely save before level switch and then load when new level comes in, so devs dont have to think about it! 
	UFUNCTION()
	virtual void OnPreWorldFinishDestroy(UWorld* World);

	UFUNCTION()
	virtual void OnPostWorldCreation(UWorld* World);

	bool bSavingDisabled; 

	FTimerHandle TimerHandle_DeferredLoadPlayerData;

private:

	TSubclassOf<class UNarrativeSave> GetSaveGameClass() const;

	TMap<FGuid, TWeakObjectPtr<class AActor>> QuickLookupMap;

};
