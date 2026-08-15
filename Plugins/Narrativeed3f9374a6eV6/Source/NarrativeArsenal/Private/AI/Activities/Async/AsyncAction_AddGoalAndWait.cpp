// Copyright Narrative Tools 2025.

#include "AI/Activities/Async/AsyncAction_AddGoalAndWait.h"
#include "AI/Activities/NPCActivityComponent.h"

void UAsyncAction_AddGoalAndWait::SetReadyToDestroy()
{
	if (CachedNPCGoal)
	{
		CachedNPCGoal->OnGoalSucceeded.RemoveAll(this);
		CachedNPCGoal->OnGoalRemoved.RemoveAll(this);
	}
	
	Super::SetReadyToDestroy();
}

UAsyncAction_AddGoalAndWait* UAsyncAction_AddGoalAndWait::AddGoalAndWait(UNPCActivityComponent* NPCActivityComponent, UNPCGoalItem* NewGoal, const bool bTriggerReselect, const bool bPersistant, UNPCGoalItem*& Goal)
{
	UAsyncAction_AddGoalAndWait* Action = NPCActivityComponent && NewGoal? NewObject<UAsyncAction_AddGoalAndWait>() : nullptr;
	if (Action)
	{
		if (bPersistant)
		{
			Action->RegisterWithGameInstance(NPCActivityComponent);
		}

		Goal = NPCActivityComponent->AddGoal(NewGoal, bTriggerReselect);
		if (Goal)
		{
			Action->CachedNPCGoal = Goal;
			Goal->OnGoalSucceeded.AddDynamic(Action, &UAsyncAction_AddGoalAndWait::OnGoalSucceeded);
			Goal->OnGoalRemoved.AddDynamic(Action, &UAsyncAction_AddGoalAndWait::OnGoalRemoved);
		}
		else // no goal, end task
		{
			Action->EndTask();
		}
	}

	return Action;
}

void UAsyncAction_AddGoalAndWait::OnGoalSucceeded(UNPCActivity* Activity, UNPCGoalItem* Goal)
{
	if (GoalSucceeded.IsBound())
	{
		GoalSucceeded.Broadcast(Activity);

		// goal succeeded, no longer need events.
		EndTask();
	}
}

void UAsyncAction_AddGoalAndWait::OnGoalRemoved(UNPCActivity* Activity, UNPCGoalItem* Goal)
{
	if (GoalRemoved.IsBound())
	{
		GoalRemoved.Broadcast(Activity);

		// goal succeeded, no longer need events.
		EndTask();
	}
}
