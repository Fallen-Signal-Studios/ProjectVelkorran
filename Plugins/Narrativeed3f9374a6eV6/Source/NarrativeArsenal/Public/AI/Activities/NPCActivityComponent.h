// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPCActivity.h"
#include "NarrativeSavableComponent.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "NPCGoal.h"
#include "NPCGoalItem.h"
#include "NPCGoalGenerator.h"
#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategory.h"
#endif
#include "NPCActivityComponent.generated.h"

#if WITH_GAMEPLAY_DEBUGGER

class AActor;
class APlayerController;

class FGameplayDebuggerCategory_ActivityComponent : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_ActivityComponent();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

};

#endif // WITH_GAMEPLAY_DEBUGGER

/**Lives on the NPCCharacter and allows us to run NPC activities. For more info on NPC Actitites see the comment above UNPCActivity.  */
UCLASS( ClassGroup=(Narrative), meta=(BlueprintSpawnableComponent))
class NARRATIVEARSENAL_API UNPCActivityComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()

public:	

	friend class ANarrativeNPCController;
	// Private, read-only non-Shipping diagnostics; no public mutable AI access.
	friend struct FNarrativeAIStartupDiagnostics;

	// Sets default values for this component's properties
	UNPCActivityComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void RescoreGoals();

	/**
	 * An actor's faction membership changed, so an attitude this NPC already resolved may now
	 * be wrong. Narrowed to actors this NPC is actually perceiving - an NPC that cannot see the
	 * actor has no stale decision about it, and will evaluate correctly when it next does.
	 */
	UFUNCTION()
	void HandleFactionMembershipChanged(AActor* Actor, FGameplayTagContainer NewFactions);

	FTimerHandle TimerHandle_RescoreGoals;

	virtual void Activate(bool bReset=false);
	virtual void Deactivate();

	virtual void Load_Implementation() override;
	virtual bool ValidateSaveRecord(const TArray<uint8>& RecordBytes) const override;
	virtual bool WasSaveRecordLoadAccepted() const override { return bSavedActivityLoadAccepted; }
	/** Read-only completion gate for managed world restoration. Check acceptance first. */
	bool HasPendingSavedActivityRestore() const { return bSavedActivityRestorePending || bApplyingSavedActivities; }
	virtual void PrepareForSave_Implementation() override; 

	#if ENABLE_VISUAL_LOG
	/** prepare blackboard snapshot for logs */
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif

#if WITH_GAMEPLAY_DEBUGGER
	virtual void DescribeSelfToGameplayDebugger(FGameplayDebuggerCategory* DebuggerCategory) const;
#endif // WITH_GAMEPLAY_DEBUGGER

	/*Scores our activities, and selects the best one, along with the best goal for that activity. 
	@param bCheckNew will potentially end CurrentActivity if a better one is found, otherwise if a valid current activity is running
	that will be kept instead.
	
	@return whether a new activity was selected or not 
	*/
	UFUNCTION(BlueprintCallable, Category = "Activities")
	bool PerformActivitySelection(bool bCheckNew=false);

	//Check if we can run an activity, without actually running it. 
	bool CanRunActivity(UNPCActivity* ActivityTemplate,  UNPCGoalItem* Goal, FString& FailReason) const;

	/** Find an added activity via its class */
	UFUNCTION(BlueprintPure, Category = "Activities")
	UNPCActivity* GetActivity(TSubclassOf<UNPCActivity> ActivityClass);

	/** Add an activity to our list. */
	UFUNCTION(BlueprintCallable, Category = "Activities")
	UNPCActivity* AddActivity(TSubclassOf<UNPCActivity> ActivityClass, const bool bSaveActivity);

	/** Remove an activity from our list. */
	UFUNCTION(BlueprintCallable, Category = "Activities")
	bool RemoveActivity(TSubclassOf<UNPCActivity> ActivityClass);
	
	/** Find an added GoalGenerator via its class */
	UFUNCTION(BlueprintPure, Category = "Activities")
	UNPCGoalGenerator* GetGoalGenerator(TSubclassOf<UNPCGoalGenerator> GoalGeneratorClass);

	/** Add an GoalGenerator to our list. */
	UFUNCTION(BlueprintCallable, Category = "Activities")
	UNPCGoalGenerator* AddGoalGenerator(TSubclassOf<UNPCGoalGenerator> GoalGeneratorClass, const bool bSaveGoalGenerator);

	/** Remove n GoalGenerator from our list. */
	UFUNCTION(BlueprintCallable, Category = "Activities")
	bool RemoveGoalGenerator(TSubclassOf<UNPCGoalGenerator> GoalGeneratorClass);

	/**
	 * Ask every generator to reconsider the actors this NPC already perceives.
	 *
	 * Server-authoritative: goals are produced on the authority and replicated, so this does
	 * nothing without it. Safe to call repeatedly - generators are required to be idempotent,
	 * and goal admission rejects a duplicate for an already-registered key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Activities")
	void ReevaluatePerceivedActors();

	/** Start the given activity, and pass the goal to it. Goal can be nullptr  */
	UFUNCTION(BlueprintCallable, Category = "Activities")
	bool RunActivity(UNPCActivity* ActivityTemplate, UNPCGoalItem* Goal, FString& FailReason);

	//Add an activity schedule to the NPC. 
	UFUNCTION(BlueprintCallable, Category = "Activities")
	void AddActivitySchedule(class UNPCActivitySchedule* Schedule);

	//Remove an activity schedule from the NPC. 
	UFUNCTION(BlueprintCallable, Category = "Activities")
	void RemoveActivitySchedule(class UNPCActivitySchedule* Schedule);

	//Set our activity config
	UFUNCTION(BlueprintCallable, Category = "Activities")
	void SetActivityConfiguration(class UNPCActivityConfiguration* Config);

	//Stop the currently running activity
	UFUNCTION(BlueprintCallable, Category = "Activities")
	void StopCurrentActivity();

	//get our current activity
	UFUNCTION(BlueprintPure, Category = "Activities")
	FORCEINLINE UNPCActivity* GetCurrentActivity() const {return CurrentActivity;};

	//get our current activities goal 
	UFUNCTION(BlueprintPure, Category = "Activities")
	FORCEINLINE UNPCGoalItem* GetCurrentActivityGoal() const {return CurrentActivity ? CurrentActivity->ActivityGoal : nullptr;};

	//GOALS


	/*Add the given goal to the goal map using its goaltag.Return a handle to the created goal
	
	@param bTriggerReselect whether you want to ask the activity component to reselect its behavior after adding this goal. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Goals")
	UNPCGoalItem* AddGoal(UNPCGoalItem* NewGoal, const bool bTriggerReselect=false);

	/** Native-only admission: no Blueprint callbacks. Keyless goals remain valid.
	 * A goal registered against an object cannot outlive that exact object. */
	bool HasStaleRegisteredGoalKey(const UNPCGoalItem* Goal) const;

	//Remove the goal with the given handle 
	UFUNCTION(BlueprintCallable, Category = "Goals")
	void RemoveGoal(UNPCGoalItem* GoalToRemove);

	//Remove all goals from the ac. 
	UFUNCTION(BlueprintCallable, Category = "Goals")
	void RemoveAllGoals();

	//Grab all created goals of the given type
	UFUNCTION(BlueprintPure, Category = "Goals")
	FNPCGoalContainer GetGoals(const TSubclassOf<UNPCGoalItem>& GoalType) const;

	//Return true if we have a goal of the given type 
	UFUNCTION(BlueprintPure, Category = "Goals")
	bool HasGoal(const TSubclassOf<UNPCGoalItem>& GoalType) const;

	//Grab a goal via its key 
	UFUNCTION(BlueprintPure, Category = "Goals", meta = (DeterminesOutputType = "GoalType"))
	UNPCGoalItem* GetGoalByKey(const TSubclassOf<UNPCGoalItem>& GoalType, const UObject* Key, bool& OutSucceeded);

protected:

	// Actor-backed registered goals retire at destruction, before their own
	// Blueprint timers can query a stale target. No saved fields or new tick.
	void RefreshGoalKeyActorBindings();
	UFUNCTION()
	void OnGoalKeyActorDestroyed(AActor* DestroyedActor);
	TSet<TWeakObjectPtr<AActor>> BoundGoalKeyActors;

	// Saved component bytes can arrive during GameMode::InitGame, before the
	// controller/component BeginPlay owner cache exists. Keep them intact until
	// the real controller finishes initialization; never synthesize an owner.
	void QueueSavedActivityRestore();
	void RestoreSavedActivities();
	bool OwnsSavedActivityRestore(uint64 Generation) const;
	FTimerHandle TimerHandle_SavedActivityRestore;
	TWeakObjectPtr<ANarrativeNPCController> SavedActivityLoadController;
	TWeakObjectPtr<APawn> SavedActivityLoadPawn;
	uint64 SavedActivityLoadPawnGeneration = 0;
	uint64 SavedActivityLoadGeneration = 0;
	bool bSavedActivityRestorePending = false;
	bool bApplyingSavedActivities = false;
	bool bSavedActivityLoadAccepted = true;
	bool bActivityComponentEndingPlay = false;

	void StopActivity_Internal(UNPCActivity* Activity, bool bCleanupBT = true);

	bool StartActivity(UNPCActivity* Activity,  UNPCGoalItem* Goal, FString& FailReason);

	//Our controller, cached
	UPROPERTY(BlueprintReadOnly, Category = "Activities")
	TObjectPtr<ANarrativeNPCController> OwnerController;

	//How often to rescore activities
	UPROPERTY(EditDefaultsOnly, SaveGame, BlueprintReadOnly, Category = "Activities")
	float RescoreInterval;

	//Our activity configuration goes in here
	UPROPERTY(EditDefaultsOnly, SaveGame, Category = "Activities")
	TObjectPtr<class UNPCActivityConfiguration> ActivityConfiguration;

	//The activities the NPC can run. 
	UPROPERTY(Instanced, EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	TArray<TObjectPtr<class UActivityGroup>> ActivityGroups; 

	//The activities the NPC can run. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	TArray<TObjectPtr<class UNPCActivity>> Activities; 

	//Our goal generators we'll use to create goals - goals can also be explicitly added 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	TArray<TObjectPtr<class UNPCGoalGenerator>> GoalGenerators; 

	//Our goals we currently have - we use a map to allow quick access to goals by their class 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Activity Group")
	TMap<TSubclassOf<UNPCGoalItem>, FNPCGoalContainer> Goals; 

	UPROPERTY() 
	TArray<TObjectPtr<class UScheduledBehavior_NPC>> ActiveScheduledActivites;

	///**Some goals need to be serialized to disk, for example if our player asks an NPC to follow them, the NPC needs to
	//remember that goal that when our game loads back in. */
	UPROPERTY(SaveGame, VisibleAnywhere, Category = "Activities")
	TArray<FSavedGoalItem> SavedGoals;

	/* The NPCs activities, serialized so we can load them back in. */
	UPROPERTY(SaveGame, VisibleAnywhere, Category = "Activities")
	TArray<FSavedNPCActivity> SavedActivities;

	/* Saved goal generators */
	UPROPERTY(SaveGame, VisibleAnywhere, Category = "Activities")
	TArray<FSavedNPCGoalGenerator> SavedGoalGenerators;

	//The activity we're currently running
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Activities")
	TObjectPtr<UNPCActivity> CurrentActivity;

//public:
//
//	virtual void GetSaveGoals(TArray<FSavedGoalItem>& OutSaveGoals);
//	virtual void LoadSavedGoals(const TArray<FSavedGoalItem>& OutSaveGoals);

};
