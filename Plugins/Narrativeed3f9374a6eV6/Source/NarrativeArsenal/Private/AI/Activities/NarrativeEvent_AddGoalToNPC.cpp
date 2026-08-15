// Copyright Narrative Tools 2024. 


#include "AI/Activities/NarrativeEvent_AddGoalToNPC.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "AI/NarrativeCharacterSubsystem.h"

UNarrativeEvent_AddGoalToNPC::UNarrativeEvent_AddGoalToNPC(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	EventFilter = EEventFilter::EF_OnlyNPCs;
}

void UNarrativeEvent_AddGoalToNPC::OnActivate_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Target))
	{
		if (UNPCActivityComponent* ActivityComponent = NPC->GetActivityComponent())
		{

			//If we've already started this, dont start it again 
			if (!ActiveGoal)
			{
				ActiveGoal = ActivityComponent->AddGoal(GoalToAdd);
			}
		}
	}
}

void UNarrativeEvent_AddGoalToNPC::OnDeactivate_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (ActiveGoal)
	{
		if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Target))
		{
			if (UNPCActivityComponent* ActivityComponent = NPC->GetActivityComponent())
			{
				ActivityComponent->RemoveGoal(ActiveGoal);
				ActiveGoal = nullptr;
			}
		}
	}
}

FString UNarrativeEvent_AddGoalToNPC::GetGraphDisplayText_Implementation()
{
	if (GoalToAdd)
	{
		FString ActivityName = GoalToAdd ? GoalToAdd->GetDebugString() : FString();
		FString NPCNames;
		for (auto& NPC : NPCTargets)
		{
			if (NPC)
			{
				NPCNames += (NPC->NPCName.ToString() + ',');
			}
		}

		if (!NPCNames.Len())
		{
			NPCNames = "Owner";
		}

		return FString::Printf(TEXT("Add Goal: %s with score %f to %s"), *ActivityName, GoalToAdd->DefaultScore, *NPCNames);
	}

	return "";
}

void UNarrativeEvent_AddGoalToNPC::ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	OnActivate(Pawn, Controller, NarrativeComponent);
}

UNarrativeEvent_AddGoalMulti::UNarrativeEvent_AddGoalMulti(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UNarrativeEvent_AddGoalMulti::OnActivityCompleted(class UNPCActivity* Activity, class UNPCGoalItem* Goal)
{
	for (auto& Target : NPCGoalTargets)
	{
		if (Target.Goal == Goal)
		{
			Target.bGoalSucceeded = true; 
		}
	}

	for (auto& Target : NPCGoalTargets)
	{
		if (!Target.bGoalSucceeded)
		{
			return;
		}
	}

	//All our goals have been succeeded! Do whatever we like now. 
	OnGoalsCompleted();
}

void UNarrativeEvent_AddGoalMulti::OnGoalsCompleted_Implementation()
{
	//Do whatever we like in here. 
}

FString UNarrativeEvent_AddGoalMulti::GetGraphDisplayText_Implementation()
{
	FString ActivityDesc = "Create Goals for: ";

	for (auto& Target : NPCGoalTargets)
	{
		if (Target.NPCDefinition)
		{
			ActivityDesc += Target.NPCDefinition->NPCName.ToString() + ", ";
		}
	}

	return ActivityDesc;
}

void UNarrativeEvent_AddGoalMulti::OnDeactivate_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (UNarrativeCharacterSubsystem* NPCS = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
	{
		for (auto& NPCTarget : NPCGoalTargets)
		{
			if (ANarrativeNPCCharacter* NPCChar = NPCS->FindNPC(NPCTarget.NPCDefinition))
			{
				if (UNPCActivityComponent* ActivityComponent = NPCChar->GetActivityComponent())
				{
					ActivityComponent->RemoveGoal(NPCTarget.Goal);
				}
			}
		}
	}

}

void UNarrativeEvent_AddGoalMulti::OnActivate_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	if (UNarrativeCharacterSubsystem* NPCS = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
	{
		bool bSucceeded = true; 

		for (auto& NPCTarget : NPCGoalTargets)
		{
			if (ANarrativeNPCCharacter* NPCChar = NPCS->FindNPC(NPCTarget.NPCDefinition))
			{
				if (UNPCActivityComponent* ActivityComponent = NPCChar->GetActivityComponent())
				{	
					//If any NPCs fail to run the activity and require succeed, don't run the settlement activity
					FString FailReason;

					UNPCGoalItem* Goal = ActivityComponent->AddGoal(NPCTarget.Goal);

					//If the NPCs didnt act on the goal then they had something better to do, then we have failed. 
					if (ActivityComponent->GetCurrentActivityGoal() != Goal && NPCTarget.bRequireSucceed)
					{
						ActivityComponent->RemoveGoal(Goal);
						bSucceeded = false;
						break;
					}

					IssuedGoals.Add(Goal);
					Goal->OnGoalSucceeded.AddDynamic(this, &UNarrativeEvent_AddGoalMulti::OnActivityCompleted);
				}
			}
			else if(NPCTarget.bRequireSucceed)
			{
				//If any NPC couldn't be found, don't run the activity - TODO we may want to allow, should make configurable. 
				bSucceeded = false;
				break;
			}
		}

		//If we failed, remove any goals we created. 
		if (!bSucceeded)
		{
			OnDeactivate(Target, Controller, NarrativeComponent);
		}

	}
}

void UNarrativeEvent_AddGoalMulti::ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	OnActivate(Pawn, Controller, NarrativeComponent);
}
