// Copyright Narrative Tools 2024. 


#include "AI/Activities/NPCActivity.h"
#include <AbilitySystemComponent.h>
#include "AI/Activities/NPCActivityComponent.h"
#include "NarrativeLogChannels.h"

UNPCActivity::UNPCActivity(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsInterruptable = false; 
	LastScore = 0.f;
}

bool UNPCActivity::IsActivityActive(float& OutActiveTime) const
{
	OutActiveTime = FLT_MAX;
	
	if (LastActivateTime > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			OutActiveTime = World->TimeSince(LastActivateTime);
		}
	}

	return OwnerActivityComponent ? OwnerActivityComponent->GetCurrentActivityGoal() == ActivityGoal : false;
}

void UNPCActivity::SetOwner(class ANarrativeNPCController* InOwnerController, class UNPCActivityComponent* InOwnerComp)
{
	check(InOwnerController && InOwnerComp);
	OwnerController = InOwnerController;
	OwnerActivityComponent = InOwnerComp;
	LastScore = -1.f;
	LastActivateTime = -1.f; 
}

bool UNPCActivity::SetupBlackboard_Implementation(class UBlackboardComponent* BB)
{
	return true;
}

float UNPCActivity::ScoreActivity_Implementation(const FNPCGoalContainer& GoalContainer, UNPCGoalItem*& OutBestGoal, TArray<UNPCGoalItem*>& OutInvalidGoals)
{
	//If we have no goals, return <0, this activity shouldnt be doable 
	if (GoalContainer.IsEmpty())
	{
		return 0.f;
	}

	float BestScore = 0.f;
	UNPCGoalItem* BestGoalItem = nullptr; 

	for (auto& Goal : GoalContainer.Goals)
	{
		const float GoalScore = ScoreGoalItem(Goal);

		//Negative scored goals should be removed, and zero scored goals should be ignored. 
		if (GoalScore < 0.f)
		{
			OutInvalidGoals.Add(Goal);
		}
		else if (!FMath::IsNearlyZero(GoalScore) && GoalScore > BestScore)
		{
			BestGoalItem = Goal;
			BestScore = GoalScore; 
		}
	}

	OutBestGoal = BestGoalItem;
	return BestScore; 
}

float UNPCActivity::ScoreGoalItem_Implementation(const UNPCGoalItem* Goal) 
{
	if (Goal)
	{
		//Check if goals lifetime is up 
		if (Goal->GoalLifetime > 0.f)
		{
			if (UWorld* World = GetWorld())
			{
				if (World->TimeSince(Goal->CreationTime) >= Goal->GoalLifetime)
				{
					return -1.f;
				}
			}
		}

		//Tag checks - score zero so we dont use the goal if we're missing the tags 
		if (OwnerController)
		{
			//Tag checks
			if (OwnerController->HasAnyMatchingGameplayTags(Goal->BlockTags))
			{
				return 0.f;
			}

			if (!OwnerController->HasAllMatchingGameplayTags(Goal->RequireTags))
			{
				return 0.f;
			}
		}

 		return Goal->GetGoalScore();
	}

	return -FLT_MAX; 
}

bool UNPCActivity::RunActivity()
{	
	if (OwnerController && BehaviourTree)
	{
		if (OwnerController->RunBehaviorTree(BehaviourTree))
		{
			if (SetupBlackboard(OwnerController->GetBlackboardComponent()))
			{
				if (UAbilitySystemComponent* ASC = OwnerController->GetAbilitySystemComponent())
				{
					ASC->AddLooseGameplayTags(OwnedTags, 1);

					if (ActivityGoal)
					{
						ASC->AddLooseGameplayTags(ActivityGoal->OwnedTags, 1);
					}
				}

				LastActivateTime = GetWorld()->GetTimeSeconds();

				return true;
			}
			else
			{
				UE_LOG(LogNarrativeActivityComponent, Display, TEXT("Failed to run activity %s as SetupBlackboard() failed. "), *GetNameSafe(this));
				//Blackboard failed to initialize, stop the behaviour
				OwnerController->StopBehaviorTree();
			}
		}
	}

	return false; 
}

bool UNPCActivity::EndActivity()
{
	if (OwnerController)
	{
		if (UAbilitySystemComponent* ASC = OwnerController->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTags(OwnedTags, 1);

			if (ActivityGoal)
			{
				ASC->RemoveLooseGameplayTags(ActivityGoal->OwnedTags, 1);
			}
		}
	}

	return true; 
}

void UNPCActivity::StopBehaviorTree()
{
 	OwnerController->StopBehaviorTree();
}

void UNPCActivity::NotifySucceeded()
{
	OnActivityGoalSucceeded.Broadcast(this, ActivityGoal);

	if (ActivityGoal)
	{
		ActivityGoal->OnGoalSucceeded.Broadcast(this, ActivityGoal);

		if (ActivityGoal->bRemoveOnSucceeded)
		{
			RemoveActivityGoal();
		}
	}
}

void UNPCActivity::RemoveActivityGoal()
{
	//Remove our goal, making the activity end 
	if (OwnerActivityComponent && ActivityGoal)
	{		
		ActivityGoal->OnGoalRemoved.Broadcast(this, ActivityGoal);
		OwnerActivityComponent->RemoveGoal(ActivityGoal);
	}
}
