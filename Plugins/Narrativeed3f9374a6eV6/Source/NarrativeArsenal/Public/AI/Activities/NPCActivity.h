// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AI/NarrativeNPCController.h"
#include <UObject/Object.h>
#include "UnrealFramework/NarrativePlayerController.h"
#include "NarrativeActivityBase.h"
#include "NPCGoalItem.h"
#include "NPCActivity.generated.h"

//A saved NPC Activity - which consists of the class and the uint8 data to serialize
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSavedNPCActivity
{

	GENERATED_USTRUCT_BODY()

	FSavedNPCActivity(){};

	UPROPERTY(SaveGame)
	TSubclassOf<UNPCActivity> Class;

	UPROPERTY(SaveGame)
	TArray<uint8> Data;
};

/**
 * NPC Activities essentially wrap a Behaviour tree, and act on UGoalItems, which define how we'll set up the behavior trees blackboard. 
 * See UNPCActivityComponent.PerformActivitySelection to see how AI select what activity to perform. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVEARSENAL_API UNPCActivity : public UNarrativeActivityBase
{
	GENERATED_BODY()
	
public:

	UNPCActivity(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "NPC Activity")
	FORCEINLINE bool IsInterruptable() const {return bIsInterruptable;};

	//Return whether the activity is active or not. ActiveTime is how long the activity has been active, or the time since it went deactive. Will be FLT_MAX if has never been activated.  
	UFUNCTION(BlueprintPure, Category = "NPC Activity")
	virtual bool IsActivityActive(float& OutActiveTime) const;

	void SetOwner(class ANarrativeNPCController* OwnerController, class UNPCActivityComponent* OwnerComp);

	UPROPERTY()
	float LastScore;

protected:

	friend class UNPCActivityComponent;

	/**Once the activity starts, this will be called to try and set up the blackboard. You can return false
	if the blackboard wasn't able to be setup and RunActivity will then return false to let the call know it failed. */
	UFUNCTION(BlueprintNativeEvent)
	bool SetupBlackboard(class UBlackboardComponent* BB);
	virtual bool SetupBlackboard_Implementation(class UBlackboardComponent* BB);
	
	/** Score the activity - the activity with the best score will be ran using its BestGoal. Invalid goals will also be removed.  */
	UFUNCTION(BlueprintNativeEvent)
	float ScoreActivity(const FNPCGoalContainer& GoalContainer, UNPCGoalItem*& OutBestGoal, TArray<UNPCGoalItem*>& OutInvalidGoals);
	virtual float ScoreActivity_Implementation(const FNPCGoalContainer& GoalContainer, UNPCGoalItem*& OutBestGoal, TArray<UNPCGoalItem*>& OutInvalidGoals);

	/**Override this if you want to ignore Goal.GetScore() and provide a custom score. IE you may want a SitOnCouch goal 
	to score higher if the owner has low stamina, but lower or zero if we have enough stamina. */
	UFUNCTION(BlueprintNativeEvent)
	float ScoreGoalItem(const UNPCGoalItem* Goal);
	virtual float ScoreGoalItem_Implementation(const UNPCGoalItem* Goal);

	//Tell our owner controller to start/stop the activity
	virtual bool RunActivity() override;
	virtual bool EndActivity() override;
	virtual void StopBehaviorTree();

	//Tell the activity its succeeded. Ie for attacking this may be when target is killed, or for moving somewhere this might be when we reach the destination. 
	UFUNCTION(BlueprintCallable, Category = "NPC Activity")
	virtual void NotifySucceeded();

	UFUNCTION(BlueprintCallable, Category = "NPC Activity")
	virtual void RemoveActivityGoal();

	//Broadcast when the activity is completed. 
	UPROPERTY(BlueprintAssignable, Category = "NPC Activity")
	FOnGoalSignature OnActivityGoalSucceeded;

	//We cache the AI controller 
	UPROPERTY(BlueprintReadOnly, Transient, Category = "NPC Activity")
	TObjectPtr<ANarrativeNPCController> OwnerController;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "NPC Activity")
	TObjectPtr<class UNPCActivityComponent> OwnerActivityComponent;

	//The goal this activity is operating on - for example AttackGoal for AttackActivity, etc. This can null if your activity doesn't need a goal 
	UPROPERTY(BlueprintReadOnly, Transient, Category = "NPC Activity")
	TObjectPtr<class UNPCGoalItem> ActivityGoal;

	//The behaviour tree the NPC needs to run
	UPROPERTY(EditDefaultsOnly, Category = "NPC Activity")
	TObjectPtr<class UBehaviorTree> BehaviourTree;

	//The goal class this activity supports, if it supports one. You can leave this empty if your activity doesn't need a goal, eg Idle, etc. 
	UPROPERTY(EditDefaultsOnly, Category = "NPC Activity")
	TSubclassOf<class UNPCGoalItem> SupportedGoalType;

	//Whether we're allowed to interrupt this activity or not 
	UPROPERTY(EditDefaultsOnly, Category = "NPC Activity")
	bool bIsInterruptable;
	
	//Whether this activity should be saved to disk or not - this is set when you call AddActivity. 
	UPROPERTY(BlueprintReadOnly, Category = "NPC Activity")
	bool bSaveActivity;

	//Whether this activity should be saved to disk or not
	UPROPERTY(BlueprintReadOnly, Category = "NPC Activity")
	float LastActivateTime;
};
