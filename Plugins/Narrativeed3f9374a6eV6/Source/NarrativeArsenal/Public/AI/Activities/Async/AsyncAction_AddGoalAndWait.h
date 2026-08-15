// Copyright Narrative Tools 2025.

#pragma once

#include "AI/Activities/NPCActivity.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncAction_AddGoalAndWait.generated.h"

class UNPCGoalItem;
class UNPCActivityComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAddGoalAndWaitSignature, UNPCActivity*, Activity);

// adds a goal for a given npc and calls goal succeeded when goal succeeded 
UCLASS(MinimalAPI, BlueprintType, meta=(ExposedAsyncProxy="AsyncTask"))
class UAsyncAction_AddGoalAndWait : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	// new goal item
	UPROPERTY()
	TObjectPtr<UNPCGoalItem> CachedNPCGoal;

	// called once the goal succeeds
	UPROPERTY(BlueprintAssignable)
	FAddGoalAndWaitSignature GoalSucceeded;	

	// called when goal is removed. if the goal succeeds, then this is not called.
	UPROPERTY(BlueprintAssignable)
	FAddGoalAndWaitSignature GoalRemoved;
	
public:

	/* UBlueprintAsyncActionBase */
	virtual void SetReadyToDestroy() override;
	/* UBlueprintAsyncActionBase */

	/**
	 * Add the given goal to the goal map using its goal tag.
	 * @param NPCActivityComponent owning NPC activity component
	 * @param NewGoal the new goal to use
	 * @param bTriggerReselect whether you want to ask the activity component to reselect its behavior after adding this goal. 
	 * @param bPersistant when true, the action stay alive until EndTask is manually called, allowing it to be used inside loops or if the calling BP goes away
	 * @param Goal a handle to the created goal
	 * @return the new task
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly="true", Category="Activities", AdvancedDisplay="bPersistant"))
	static UAsyncAction_AddGoalAndWait* AddGoalAndWait(UPARAM(DisplayName="NPC Activity Component") UNPCActivityComponent* NPCActivityComponent, UNPCGoalItem* NewGoal, const bool bTriggerReselect, const bool bPersistant, UNPCGoalItem*& Goal);

	// when called, sets the task to be ready to destroy, removing all bound events
	UFUNCTION(BlueprintCallable, Category="BeginDialogueAndWait")
	void EndTask() { SetReadyToDestroy(); }
	
private:
	
	UFUNCTION()
	void OnGoalSucceeded(UNPCActivity* Activity, UNPCGoalItem* Goal);
	
	UFUNCTION()
	void OnGoalRemoved(UNPCActivity* Activity, UNPCGoalItem* Goal);
	
};
