// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Tales/QuestSM.h"
#include "Tales/QuestTask.h"
#include "StableActor.h"
#include "Dialogue.h"
#include "Quest.generated.h"

class UDialogue;
struct FDialoguePlayParams;
class UQuest;
class UQuestState;
class UQuestBranch;
class UQuestBlueprint;
class UNarrativeDataTask;

//Quests
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestBranchCompleted, const UQuest*, Quest, const class UQuestBranch*, Branch);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestNewState, UQuest*, Quest, const UQuestState*, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnQuestTaskProgressChanged, const UQuest*, Quest, const UNarrativeTask*, ProgressedTask, const class UQuestBranch*, Branch, int32, OldProgress, int32, NewProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestTaskCompleted, const UQuest*, Quest, const UNarrativeTask*, CompletedTask, const class UQuestBranch*, Branch);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestSucceeded, const UQuest*, Quest, const FText&, QuestSucceededMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestFailed, const UQuest*, Quest, const FText&, QuestFailedMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestStarted, const UQuest*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestPostLoad, const UQuest*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestForgotten, const UQuest*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestRestarted, const UQuest*, Quest);

// Represents the state of a particular quest
UENUM(BlueprintType)
enum class EQuestCompletion : uint8
{
	QC_NotStarted UMETA(DisplayName="Not Started"),
	QC_Started UMETA(DisplayName = "Started"),
	QC_Succeded UMETA(DisplayName = "Succeeded"),
	QC_Failed  UMETA(DisplayName = "Failed")
};

class UQuest; 

//Allows blueprints to define instanced quest requirements. 
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FInstancedQuestRequirement
{
	GENERATED_BODY()


	FInstancedQuestRequirement(){};

	//The instanced goal
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category = "Requirement")
	TObjectPtr<UQuestRequirement> Requirement;

};

/** Quest Requirements can be added/removed by quests at any time to define requirements the player needs to adhere to.
For example, you could add a 'Stay Near Actor' requirement that specifies we need to stay near a certain NPC or the quest fails. 
Another example might be a 'Keep NPC Alive' requirement that fails the quest if a certain NPC dies. */
UCLASS(abstract, Blueprintable, BlueprintType, EditInlineNew)
class NARRATIVEARSENAL_API UQuestRequirement : public UObject
{

	GENERATED_BODY()

protected:

	friend class UQuest; 

	UQuestRequirement();


	// Allows the Object to get a valid UWorld from it's outer.
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	};

	//Called when this requirement is added to the quest. 
	UFUNCTION(BlueprintNativeEvent, Category = "Requirement")
	void OnAdded(UQuest* Quest);
	virtual void OnAdded_Implementation(UQuest* Quest);

	//Called when this requirement is removed from the quest. 
	UFUNCTION(BlueprintNativeEvent, Category = "Requirement")
	void OnRemoved(UQuest* Quest);
	virtual void OnRemoved_Implementation(UQuest* Quest);

public:

	//Get the quest that owns this requirement.  
	UFUNCTION(BlueprintPure, Category = "Requirement")
	virtual UQuest* GetOwningQuest() const;

};

UCLASS(Blueprintable, BlueprintType)
class NARRATIVEARSENAL_API UQuest : public UObject
{
	GENERATED_BODY()
	
protected:
	
	/**A lot of friends - but i'd rather couple UQuest tightly to these classes than allow anything to 
	let anything call any of the internal UQuest functions that really shouldn't ever be touched*/
	friend class UNarrativeTask;
	friend class UQuestBlueprintGeneratedClass;
	friend class UQuestState;
	friend class UQuestBranch;
	friend class UTalesComponent;

	UQuest();

	//Dialogue assets/nodes etc have the same name on client and server, so can be referenced over the network 
	bool IsNameStableForNetworking() const override { return true; };
	bool IsSupportedForNetworking() const override { return true; };

	//The current state the player is at in this quest
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	UQuestState* CurrentState;

	virtual UWorld* GetWorld() const override;

public:

	FORCEINLINE UQuestState* GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	class UQuestState* GetState(FName ID) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	class UQuestBranch* GetBranch(FName ID) const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
    FText GetQuestName() const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
	FText GetQuestDescription() const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void SetQuestName(const FText& NewName);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void SetQuestDescription(const FText& NewDescription);

	//Explicitly tell the quest to go to the given state
	UFUNCTION(BlueprintCallable, Category = "Quest")
	virtual void EnterState(UQuestState* NewState);
	
protected:

#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	void UpdateDialogueConnection();

#endif
	
	//Called when the quest is loaded back in from disk
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName="On Quest Post Load"))
	void BPQuestPostLoad();

	//Called when the quest is loaded back in from disk - allows you to set any quest state back up 
	virtual void QuestPostLoad();

	//Called before tasks are ready - a good place to set up data tasks depend on 
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName="Pre Quest Started"))
	void BPPreQuestStarted(const UQuest* Quest);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName="On Quest Started"))
	void BPOnQuestStarted(const UQuest* Quest);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	virtual void FailQuest(FText QuestFailedMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Failed"))
	void BPOnQuestFailed(const UQuest* Quest, const FText& QuestFailedMessage);

	/**Manually set the quest as succeeded. You'll need to provide some text for the UI as theres no node  the quest, you're manually succeeding it.*/
	UFUNCTION(BlueprintCallable, Category = "Quests")
	virtual void SucceedQuest(FText QuestSucceededMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Succeeded"))
		void BPOnQuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest New State"))
		void BPOnQuestNewState(UQuest* Quest, const UQuestState* NewState);

	UFUNCTION()
	virtual void OnQuestTaskProgressChanged(const UNarrativeTask* Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Objective Progress Made"))
	void BPOnQuestTaskProgressChanged(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress);

	UFUNCTION()
		void OnQuestTaskCompleted(const UNarrativeTask* Task, const class UQuestBranch* Branch);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Task Completed"))
		void BPOnQuestTaskCompleted(const UQuest* Quest, const UNarrativeTask* Task, const class UQuestBranch* Step);

	UFUNCTION()
	virtual void OnQuestBranchCompleted(const class UQuestBranch* Branch);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Branch Taken"))
		void BPOnQuestBranchCompleted(const UQuest* Quest, const class UQuestBranch* Branch);

	//Tell the quest that it is tracked - by default this will enable the quests navigation markers. 
	UFUNCTION(BlueprintCallable, Category = "Quest")
	virtual void SetTracked(const bool bNewTracked);
	
	virtual void EnterState_Internal(UQuestState* NewState);
	virtual void TakeBranch(UQuestBranch* Branch);

	//Initialize this quest from its blueprint generated class. Return true if successful. 
	virtual bool Initialize(class UTalesComponent* InitializingComp, const FName& QuestStartID = NAME_None);
	virtual void Deinitialize();
	virtual void DuplicateAndInitializeFromQuest(UQuest* QuestTemplate);
	
	virtual void BeginQuest(const FName& OptionalStartFromID = NAME_None);

	void HandleBeginDialogue();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Details")
	FText QuestName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Details", meta = (MultiLine = true))
	FText QuestDescription;

	//Whether or not the quest is marked as tracked. If tracked the navigation markers will be added, otherwise these will be hidden. 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Quest Details")
	bool bTracked;

	// A dialogue asset holding all dialogue from the quest. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue")
	TSubclassOf<UDialogue> QuestDialogue;

	// A dialogue asset holding all dialogue from the quest. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue")
	struct FDialoguePlayParams QuestDialoguePlayParams;

private:
	
#if WITH_EDITORONLY_DATA

	// use to check if the QuestDialogue was cleared and to know what it was to remove refs 
	UPROPERTY()
	TSubclassOf<UDialogue> LastQuestDialogue;
	
#endif

protected:
	
	// if true, the quest dialogue will resume when you load back into the game
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue")
	bool bResumeDialogueAfterLoad;

	/**Child quests don't inherit quest graph nodes, however sometimes you'd like children to inherit some states, 
	for example your parent quest could have a state in here called "RanOutOfTime", and that way any child quests
	could inherit the "RanOutOfTime" state instead of having to manually have one added to every quest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Quests")
	TArray<UQuestState*> InheritableStates;

	/**Current quest progress*/
	UPROPERTY()
	EQuestCompletion QuestCompletion;

	//The beginning state of this quest
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	UQuestState* QuestStartState;

	//Holds all of the states in the quest
	UPROPERTY()
	TArray<UQuestState*> States;

	//Holds all of the branches in the quest
	UPROPERTY()
	TArray<UQuestBranch*> Branches;

	//Holds all the spawned quest actors
	UPROPERTY(SaveGame)
	TArray<FStableActor> QuestActors;

	//Holds all the quest requirements 
	UPROPERTY()
	TArray<TObjectPtr<UQuestRequirement>> QuestRequirements;

	/**All the states we've reached so far. Useful for a quest journal, where we need to show the player what they have done so far*/
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	TArray<UQuestState*> ReachedStates;

	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	class UTalesComponent* OwningComp;

	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	class APawn* OwningPawn;

	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	class APlayerController* OwningController;

	/**Called when a quest objective has been completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestBranchCompleted QuestBranchCompleted;

	/**Called when a quest objective is updated and we've received a new objective*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestNewState QuestNewState;

	/**Called when a quest task in a branch has made progress. ie 6 out of 10 coins found*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestTaskProgressChanged QuestTaskProgressChanged;

	/**Called when a quest task on a branch is completed ie 10 out of 10 coins found*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestTaskCompleted QuestTaskCompleted;

	/**Called when a quest is completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestSucceeded QuestSucceeded;

	/**Called when a quest is failed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestFailed QuestFailed;

	/**Called when a quest is started.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestStarted QuestStarted;

		/**Called when the quest has been loaded back in.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestPostLoad OnQuestPostLoad;

	/**Called when a quest is forgotten.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestForgotten QuestForgotten;

	/**Called when a quest is restarted.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
		FOnQuestRestarted QuestRestarted;

public:

	/*
	* Spawn a quest actor! This will ensure the actors lifetime is managed automatically by the quest for you - when the quest ends, 
	* all of the spawned quest actors will be cleaned up from the level. It also handles re-spawning the actor if we load back in and quest is still active. 
	* 
	* Make sure to use the SpawnedActor ref variable to store your actor, and mark the var as SaveGame. This will let you safely access the spawned actor even if the player
	* has closed and re-opened the game later. 
	* 
	* NOTE THIS FUNCTION DOESNT DO ANYTHING IF CALLED WHILST THE QUEST IS LOADING IN! 
	* 
	* This is because any quest actor requiring respawning should be respawned via the Narrative Save Subsystem instead. If you need
	* to reference a spawned actor during a quest, store it using the SpawnedActor var and mark it as savegame - this way the reference will 
	* safely persist accross multiple play sessions. 
	* 
	* @param ActorClass this is the actor to spawn in. 
	* @param ActorTransform This is where we want to spawn the actor
	* @param SpawnedActor the spawned actor will be stored in this variable. Make sure to mark the variable as savegame so it persists across sessions. 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Quest")
	bool SpawnQuestActor(TSubclassOf<class AActor> ActorClass, FTransform ActorTransform, UPARAM(ref)FStableActor& SpawnedActor);
	virtual bool SpawnQuestActor_Implementation(TSubclassOf<class AActor> ActorClass, FTransform ActorTransform, UPARAM(ref)FStableActor& SpawnedActor);

	/* Add a requirement to the quest. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Quest")
	void AddQuestRequirement(UQuestRequirement* Requirement);
	virtual void AddQuestRequirement_Implementation(UQuestRequirement* Requirement);

	/* Remove a requirement from the quest. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Quest")
	void RemoveQuestRequirement(UQuestRequirement* Requirement);
	virtual void RemoveQuestRequirement_Implementation(UQuestRequirement* Requirement);

	void AddState(class UQuestState* State);
	void AddBranch(class UQuestBranch* Branch);

	void RemoveState(class UQuestState* State);
	void RemoveBranch(class UQuestBranch* Branch);

	void SetQuestStartState(class UQuestState* State);

	UFUNCTION(BlueprintPure, Category = "Quests")
	UQuestState* GetQuestStartState() const {return QuestStartState;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<UQuestState*> GetStates() const {return States;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<UQuestBranch*> GetBranches() const {return Branches;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<UQuestNode*> GetNodes() const;

	//Grab the completion of the quest 
	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE EQuestCompletion GetQuestCompletion() const { return QuestCompletion; };

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE class APlayerController* GetOwningController() const {return OwningController;}

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE class APawn* GetOwningPawn() const {return OwningPawn;}

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE class UTalesComponent* GetOwningComp() const {return OwningComp;};

	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE bool IsTracked() const { return bTracked;};

	/**Return all players doing this quest if shared, or the owningcontroller if solo quest*/
	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<class APlayerController*> GetGroupMembers() const;

	TSubclassOf<UDialogue> GetQuestDialogueClass() { return QuestDialogue; }
};

struct NARRATIVEARSENAL_API FQuestDelegates final
{

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnQuestStartedSignature, UTalesComponent*, UQuest*, const FName& OptionalStartFromID);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQuestEndSignature, UTalesComponent*, UQuest*);
	
	// called when a quest starts
	static FOnQuestStartedSignature OnQuestStarted;
	
	// called when a quest ends
	static FOnQuestEndSignature OnQuestEnd;
};