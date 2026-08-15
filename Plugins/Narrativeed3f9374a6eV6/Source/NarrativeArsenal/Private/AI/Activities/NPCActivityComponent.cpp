// Copyright Narrative Tools 2024. 


#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCActivitySchedule.h"
#include "AI/Activities/ActivityGroup.h"
#include "AI/Activities/NPCGoalGenerator.h"
#include <Serialization/ObjectAndNameAsStringProxyArchive.h>
#include <Serialization/MemoryReader.h>
#include <Serialization/MemoryWriter.h>
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "VisualLogger/VisualLogger.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "ArsenalStatics.h"
#include <TimerManager.h>

#include "BehaviorTree/BehaviorTreeComponent.h"

// Sets default values for this component's properties
UNPCActivityComponent::UNPCActivityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	RescoreInterval = 0.5f; 

	SetAutoActivate(true);
}

void UNPCActivityComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerController = CastChecked<ANarrativeNPCController>(GetOwner());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle_RescoreGoals, this, &UNPCActivityComponent::RescoreGoals, RescoreInterval, true);
	}

	RescoreGoals();
}

void UNPCActivityComponent::RescoreGoals()
{
	PerformActivitySelection(true);
}

void UNPCActivityComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	PerformActivitySelection();
}

void UNPCActivityComponent::Deactivate()
{
	Super::Deactivate();

	if (CurrentActivity)
	{
		StopCurrentActivity();
	}
}

#if ENABLE_VISUAL_LOG

void UNPCActivityComponent::DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory Category;
	Category.Category = "Activity Component";
	//Category.Category = FString::Printf(TEXT("Activity Component (Current Activity: %s)"), *GetNameSafe(CurrentActivity));

	for (auto& GoalKVP : Goals)
	{
		for (auto& Goal : GoalKVP.Value.Goals)
		{
			Category.Add(GetNameSafe(GoalKVP.Key), Goal->GetDebugString());
		}
	}

	if (Snapshot)
	{
		Snapshot->Status.Add(Category);
	}
}

#endif 

#if WITH_GAMEPLAY_DEBUGGER

void UNPCActivityComponent::DescribeSelfToGameplayDebugger(FGameplayDebuggerCategory* DebuggerCategory) const
{
	if (DebuggerCategory)
	{
		UWorld* World = GetWorld();

		if (!World)
		{
			return;
		}

		ANarrativeGameState* GS = Cast<ANarrativeGameState>(World->GetGameState());

		if (!GS)
		{
			return;
		}

		const float TOD = GS->GetTimeOfDay();

		//List our factions
		FString Factions;

		if (OwnerController)
		{
			if (ANarrativeNPCCharacter* NPCChar = OwnerController->GetControlledNPC())
			{		
				DebuggerCategory->AddTextLine(NPCChar->GetFactions().ToString());
			}
		}

		FString Generators = FString::Printf(TEXT("Goal Generators Added: %d | "), GoalGenerators.Num());

		for (auto& GoalGen : GoalGenerators)
		{
			if (GoalGen)
			{
				Generators += FString::Printf(TEXT("%s | "), *GetNameSafe(GoalGen));
			}
		}

		DebuggerCategory->AddTextLine(Generators);

		DebuggerCategory->AddTextLine(FString::Printf(TEXT("Activities Added: (%d)"), Activities.Num()));

		for (auto& Activity : Activities)
		{
			if (Activity)
			{
				const bool bIsCurrent = CurrentActivity && Activity->GetClass() == CurrentActivity->GetClass();

				float Score = FMath::Max(0.f, Activity->LastScore);

				if (bIsCurrent)
				{
					FString GoalStr = CurrentActivity->ActivityGoal ? CurrentActivity->ActivityGoal->GetDebugString() : "No Goal";
					DebuggerCategory->AddTextLine(FString::Printf(TEXT("{green} %s: %s | score %f"), *Activity->DescribeActivity(), *GoalStr, Score));
				}
				else if (Score > KINDA_SMALL_NUMBER)
				{
					DebuggerCategory->AddTextLine(FString::Printf(TEXT("{yellow} %s | score %f"), *Activity->DescribeActivity(), Score));
				}
				else
				{
					DebuggerCategory->AddTextLine(FString::Printf(TEXT("%s | score %f"), *Activity->DescribeActivity(), Score));
				}

			}
		}


		//if (ActiveScheduledActivites.Num())
		//{
		//	DebuggerCategory->AddTextLine(FString::Printf(TEXT("Scheduled Tasks: %d"), ActiveScheduledActivites.Num()));

		//	for (auto& SA : ActiveScheduledActivites)
		//	{
		//		if (SA)
		//		{
		//			if (UArsenalStatics::IsTimeInRange(TOD, SA->StartTime, SA->EndTime))
		//			{
		//				DebuggerCategory->AddTextLine(FString::Printf(TEXT("    {green} %.0f-%.0f: %s"), SA->StartTime, SA->EndTime, *SA->DescribeBehavior()));
		//			}
		//			else
		//			{
		//				DebuggerCategory->AddTextLine(FString::Printf(TEXT("    %.0f-%.0f: %s"), SA->StartTime, SA->EndTime, *SA->DescribeBehavior()));
		//			}
		//		}
		//	}
		//}

		for (auto& GoalKVP : Goals)
		{
			if (GoalKVP.Value.Goals.Num())
			{
				DebuggerCategory->AddTextLine(FString::Printf(TEXT("Goal Container %s:"), *GetNameSafe(GoalKVP.Key)));

				for (auto& Goal : GoalKVP.Value.Goals)
				{
					if (Goal)
					{
						if (CurrentActivity && Goal == CurrentActivity->ActivityGoal)
						{
							DebuggerCategory->AddTextLine(FString::Printf(TEXT("{yellow}   %s | Score %f"), *Goal->GetDebugString(), Goal->GetGoalScore()));
						}
						else
						{
							DebuggerCategory->AddTextLine(FString::Printf(TEXT("   %s | Score %f"), *Goal->GetDebugString(), Goal->GetGoalScore()));
						}
					}
				}
			}
		}

		if (CurrentActivity)
		{
			DebuggerCategory->AddTextLine(FString::Printf(TEXT("Current Activity %s:"), *GetNameSafe(CurrentActivity)));
		}

	}
}

#endif 

bool UNPCActivityComponent::PerformActivitySelection(bool bCheckNew)
{
	if (!IsActive())
	{
		return false;
	}

	FString FailReason;

	/**Check what activity we need to run. Fall back to Fallback activity if we don't have a valid one we can use.

	ActivityGroups hold activities and have preconditions about whether we can run the activity group. This allows us to easily early out
	on a bunch of activities. For example the Attack Activity Group may have many attacks activities in it, but we won't need to check any of them if we
	dont have a valid attack target.

	We ask all groups for valid activities we can run. These activities can also provide a goal item for them to act on. For example,
	an Attack Activity might be valid, because we have an Attack Goal. We'll ask the Attack Goal for its best attack target, and we'll
	feed that into the activity. For example RunActivity(AttackActivity, BestAttackTarget)

	Activities have a GoalClass variable that we use to see which activity maps to which goal - we use that to find the goal items.
	*/
	if (CurrentActivity && CurrentActivity->ActivityGoal)
	{
		UNPCGoalItem* OldBestItem = CurrentActivity->ActivityGoal;
		UNPCGoalItem* BestItem = nullptr;
		TArray<UNPCGoalItem*> InvalidGoals;
		const float CurrentScore = CurrentActivity->ScoreActivity(GetGoals(CurrentActivity->SupportedGoalType), BestItem, InvalidGoals);

		for (auto& InvalidGoal : InvalidGoals)
		{
			if (InvalidGoal)
			{
				RemoveGoal(InvalidGoal);
			}
		}

		//If we have an activity scoring above 0 and its goal hasn't changed, we're fine to continue on 
		if (CurrentActivity && CurrentScore > 0.f && !bCheckNew)
		{
			return false;
		}
	}

	//Moving away from groups 
	TArray<UNPCActivity*> ValidActivities = Activities;

	//for (auto& Group : ActivityGroups)
	//{
	//	if (Group)
	//	{
	//		Group->GetActivitesInGroup(ValidActivities);
	//	}
	//}

	//Lets find the best activity to run, and the best goal to give the activity  
	UNPCActivity* ActivityToRun = nullptr;
	UNPCGoalItem* GoalParam = nullptr;

	//Ask all activities for a score
	float BestScore = -1.f;

	for (auto& Activity : ValidActivities)
	{
		if (Activity)
		{
			UNPCGoalItem* BestGoal = nullptr;
			TArray<UNPCGoalItem*> InvalidGoals;
			const float Score = Activity->ScoreActivity(GetGoals(Activity->SupportedGoalType), BestGoal, InvalidGoals);

			Activity->LastScore = Score;

			for (auto& InvalidGoal : InvalidGoals)
			{
				if (InvalidGoal)
				{
					RemoveGoal(InvalidGoal);
				}
			}

			//Scores less than or equal to zero can be interrupted 
			if (Score > 0.f && Score > BestScore)
			{
				BestScore = Score;
				GoalParam = BestGoal;
				ActivityToRun = Activity;
			}
		}
	}

	//If we have nothing to run, stop all behavior. 
	if (!ActivityToRun)
	{
		// Stop current behavior tree if our requested activity BT is different
		StopActivity_Internal(CurrentActivity);
		CurrentActivity = nullptr;
	}
	else
	{
		//Dont run the same activity 
		if (CurrentActivity && ActivityToRun && CurrentActivity->GetClass() == ActivityToRun->GetClass() && GoalParam == CurrentActivity->ActivityGoal)
		{
			return false;
		}

		if (ActivityToRun)
		{
			check(OwnerController)

			bool bIsDifferentBT = CurrentActivity && ActivityToRun->BehaviourTree != CurrentActivity->BehaviourTree;

			// Stop current behavior tree if our requested activity BT is different
			StopActivity_Internal(CurrentActivity, bIsDifferentBT);
			CurrentActivity = nullptr;

			// Restart the behavior tree if we have the same BT
			auto BTComp = Cast<UBehaviorTreeComponent>(OwnerController->GetBrainComponent());
			if (!bIsDifferentBT && BTComp)
			{
				BTComp->RestartTree(EBTRestartMode::CompleteRestart);
			}

			RunActivity(ActivityToRun, GoalParam, FailReason);
			return true;
		}
	}

	return false; 
}

bool UNPCActivityComponent::CanRunActivity(UNPCActivity* ActivityTemplate, UNPCGoalItem* ActivityGoal, FString& FailReason) const
{
	if(!OwnerController || !OwnerController->HasAuthority())
	{
		FailReason = "Not Authority";
		return false; 
	}

	if (!ActivityTemplate)
	{
		FailReason = "Null template provided";
		return false; 
	}

	//Check priority - this may be removed as we move to goal-oriented 
	//if (CurrentActivity && CurrentActivity->Priority > ActivityTemplate->Priority)
	//{
	//	FailReason = "Current activity priority was higher value";
	//	return false; 
	//}

	//Tag checks
	if (OwnerController->HasAnyMatchingGameplayTags(ActivityTemplate->BlockTags))
	{
		FailReason = "Owner had blocked tag";
		return false;
	}

	if (!OwnerController->HasAllMatchingGameplayTags(ActivityTemplate->RequireTags))
	{
		FailReason = "Owner didnt have required tag";
		return false;
	}

	//Possibly ask template itself if it can run? 
	if (!ActivityTemplate->CanRunActivity(FailReason))
	{
		return false; 
	}

	return true; 
}

UNPCActivity* UNPCActivityComponent::GetActivity(TSubclassOf<UNPCActivity> ActivityClass)
{
	int32 Idx = -1;
	for (int32 i = Activities.Num() - 1; i >= 0; --i)
	{
		if (Activities.IsValidIndex(i) && Activities[i] && Activities[i]->GetClass() == ActivityClass)
		{
			Idx = i;
			break;
		}
	}

	if (Idx != -1 )
	{
		return Activities[Idx];
	}

	return nullptr; 
}

UNPCActivity* UNPCActivityComponent::AddActivity(TSubclassOf<UNPCActivity> ActivityClass, const bool bSaveActivity)
{
	//We only support adding 1 activity per class type. 
	if (!GetActivity(ActivityClass))
	{
		if (IsValid(ActivityClass))
		{
			if (UNPCActivity* ActivityToGrant = NewObject<UNPCActivity>(this, ActivityClass))
			{
				ActivityToGrant->bSaveActivity = bSaveActivity;
				Activities.AddUnique(ActivityToGrant);
				ActivityToGrant->SetOwner(OwnerController, this);
				return ActivityToGrant;
			}
		}
	}

	return nullptr; 
}

bool UNPCActivityComponent::RemoveActivity(TSubclassOf<UNPCActivity> ActivityClass)
{
	int32 Idx = -1;
	for (int32 i = Activities.Num() - 1; i >= 0; --i)
	{
		if (Activities[i] && Activities[i]->GetClass() == ActivityClass)
		{
			Idx = i;
			break;
		}
	}

	if (Idx != -1)
	{
		Activities.RemoveAt(Idx);
		return true;
	}

	return false; 
}

UNPCGoalGenerator* UNPCActivityComponent::GetGoalGenerator(TSubclassOf<UNPCGoalGenerator> GoalGeneratorClass)
{
	int32 Idx = -1;
	for (int32 i = GoalGenerators.Num() - 1; i >= 0; --i)
	{
		if (GoalGenerators.IsValidIndex(i) && GoalGenerators[i] && GoalGenerators[i]->GetClass() == GoalGeneratorClass)
		{
			Idx = i;
			break;
		}
	}

	if (Idx != -1 )
	{
		return GoalGenerators[Idx];
	}

	return nullptr; 
}

UNPCGoalGenerator* UNPCActivityComponent::AddGoalGenerator(TSubclassOf<UNPCGoalGenerator> GoalGeneratorClass, const bool bSaveGoalGenerator)
{
	if (!GetGoalGenerator(GoalGeneratorClass))
	{
		if (UNPCGoalGenerator* GoalGenCopy = NewObject<UNPCGoalGenerator>(this, GoalGeneratorClass))
		{
			GoalGenCopy->bSaveGoalGenerator = bSaveGoalGenerator;
			GoalGenerators.AddUnique(GoalGenCopy);
			GoalGenCopy->Initialize(OwnerController, this);

			return GoalGenCopy;
		}
	}

	return nullptr;

}

bool UNPCActivityComponent::RemoveGoalGenerator(TSubclassOf<UNPCGoalGenerator> GoalGeneratorClass)
{
	int32 Idx = -1;
	for (int32 i = GoalGenerators.Num() - 1; i >= 0; --i)
	{
		if (GoalGenerators[i] && GoalGenerators[i]->GetClass() == GoalGeneratorClass)
		{
			Idx = i;
			break;
		}
	}

	if (Idx != -1)
	{
		GoalGenerators.RemoveAt(Idx);
		return true;
	}

	return false; 
}

bool UNPCActivityComponent::StartActivity(UNPCActivity* NewActivity,  UNPCGoalItem* ActivityGoal, FString& FailReason)
{
	if (ANarrativeNPCCharacter* CharacterOwner = Cast<ANarrativeNPCCharacter>(GetOwner()))
	{
		OwnerController = Cast<ANarrativeNPCController>(CharacterOwner->GetController());
	}

	if (CanRunActivity(NewActivity, ActivityGoal, FailReason))
	{
		// Redundant call
		// StopCurrentActivity();

		CurrentActivity = NewActivity;

		CurrentActivity->ActivityGoal = ActivityGoal;

		if(CurrentActivity)
		{
			//Can still fail after starting, NPCs blackboard can fail to initialize itself or BP can reject start for any reason 
			const bool bSucceeded = CurrentActivity->RunActivity();
			CurrentActivity->K2_RunActivity();

			if (!bSucceeded)
			{
				StopCurrentActivity();
				return false; 
			}

			return true; 
		}
	}

	return false; 
}

bool UNPCActivityComponent::RunActivity(UNPCActivity* ActivityTemplate,  UNPCGoalItem* Goal, FString& FailReason)
{
	if (ActivityTemplate)
	{
		//Disallow restarting an already going activity 
		//if (CurrentActivity->GetClass() != ActivityTemplate->GetClass())
		{
			//TODO we probably don't need DuplicateObject here - we can reuse activities 
			
			//if (UNPCActivity* NewActivity = DuplicateObject<UNPCActivity>(ActivityTemplate, this))
			{
				ActivityTemplate->OwnerController = OwnerController;
				ActivityTemplate->OwnerActivityComponent = this;

				UNPCActivity* OldActivity = CurrentActivity;

				if (StartActivity(ActivityTemplate, Goal, FailReason))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void UNPCActivityComponent::AddActivitySchedule(class UNPCActivitySchedule* Schedule)
{
	if (Schedule)
	{
		//ActivitySchedule = Schedule;

		//ActiveScheduledActivites.Empty();

		for (auto& NPCScheduledBehavior : Schedule->Activities)
		{
			if (NPCScheduledBehavior && !NPCScheduledBehavior->bDisabled)
			{
				if (UScheduledBehavior_NPC* ScheduledActivity = DuplicateObject<UScheduledBehavior_NPC>(NPCScheduledBehavior, this))
				{
					ScheduledActivity->SetOwner(this); 
					ScheduledActivity->BindBehavior(GetWorld(), true);
					ScheduledActivity->CreatedFromSchedule = Schedule; 

					ActiveScheduledActivites.Add(ScheduledActivity);
				}
			}
		}
	}
}

void UNPCActivityComponent::RemoveActivitySchedule(class UNPCActivitySchedule* Schedule)
{
	for (auto& Activity : ActiveScheduledActivites)
	{

	}
}

void UNPCActivityComponent::SetActivityConfiguration(class UNPCActivityConfiguration* Config)
{
	if (Config)
	{
		if (Config)
		{
			//TODO we need to consider making activities instanced objects instead of classes, should try weigh up pros and cons. Also, nested activity configurations? 
			for (auto& Activity : Config->DefaultActivities)
			{
				AddActivity(Activity, true);
			}

			for (auto& GenTemplate : Config->GoalGenerators)
			{
				AddGoalGenerator(GenTemplate, true);
			}

			//This is savegame and can be changed per AC. Config just defines the default. 
			RescoreInterval = Config->RescoreInterval;
			//PerformActivitySelection();
		}
	}
}

void UNPCActivityComponent::StopCurrentActivity()
{
	StopActivity_Internal(CurrentActivity);
	CurrentActivity = nullptr; 
}

UNPCGoalItem* UNPCActivityComponent::AddGoal(UNPCGoalItem* NewGoal, const bool bTriggerReselect)
{
	if (NewGoal)
	{
		//This check basically just stops saved goals from having their creation time overriden 
		NewGoal->CreationTime = GetWorld()->GetTimeSeconds();

		const float TOD = UArsenalStatics::GetTimeOfDay(this);

		//If our goal created didnt set this assume goal was supposed to start at current time 
		if (NewGoal->IntendedTODStartTime < 0.f)
		{
			NewGoal->IntendedTODStartTime = TOD;
		}

		NewGoal->TODCreationTime = TOD;
		NewGoal->OwnerController = OwnerController;

		//We call this immediately, as GetGoalKey() may require Initialize to set up the key. 
		NewGoal->Initialize();

		//Add the goal to the set.
		if (!Goals.Contains(NewGoal->GetClass()))
		{
			FNPCGoalContainer NewGoalSet;
			NewGoalSet.Goals.Add(NewGoal);

			if (UObject* Key = NewGoal->GetGoalKey())
			{
				NewGoalSet.GoalUniqueObjectMap.Add(Key, NewGoal);
			}

			Goals.Add(NewGoal->GetClass(), NewGoalSet);
		}
		else
		{	
			//Enforce uniqueness - dont allow multiple goals with same key
			if (UObject* Key = NewGoal->GetGoalKey())
			{
				if (Goals[NewGoal->GetClass()].GoalUniqueObjectMap.Contains(Key))
				{
					return nullptr;
				}

				Goals[NewGoal->GetClass()].GoalUniqueObjectMap.Add(Key, NewGoal);
			}

			Goals[NewGoal->GetClass()].Goals.Add(NewGoal);

		}

		//let the goal set itself if needed - moved to start to key can be initialized 
		//NewGoal->Initialize();

		//By default if new goal scores higher than existing one lets perform a reselect 
		if ((!CurrentActivity || !CurrentActivity->ActivityGoal) || (CurrentActivity && CurrentActivity->ActivityGoal && NewGoal->GetGoalScore() > CurrentActivity->ActivityGoal->GetGoalScore()))
		{
			PerformActivitySelection(true);
		}
		else if (bTriggerReselect) 		//Adding a new goal can ask for a reselect if it wants. 
		{
			PerformActivitySelection();
		}

		return NewGoal; 
	}

	return nullptr;
}

void UNPCActivityComponent::RemoveGoal(UNPCGoalItem* GoalToRemove)
{
	if (GoalToRemove)
	{
		if (Goals.Contains(GoalToRemove->GetClass()))
		{
			if (Goals[GoalToRemove->GetClass()].Goals.Remove(GoalToRemove) > 0)
			{
				GoalToRemove->OnRemoved();
			}

			if (UObject* Key = GoalToRemove->GetGoalKey())
			{
				Goals[GoalToRemove->GetClass()].GoalUniqueObjectMap.Remove(Key);
			}

			

			//If we're removing our current activities goal, the activity needs to end and we need a reselect 
			if (CurrentActivity && CurrentActivity->ActivityGoal == GoalToRemove)
			{
				// Moving this to within PerformActivitySelection in case the same BT wants to be executed
				//StopCurrentActivity();

				PerformActivitySelection();
			}
		}
	}
}

void UNPCActivityComponent::RemoveAllGoals()
{
	for (auto& GoalSet : Goals)
	{
		auto& GoalContainer = GoalSet.Value;

		for (auto& Goal : GoalContainer.Goals)
		{
			Goal->OnRemoved();
		}

		GoalContainer.Goals.Empty();
		GoalContainer.GoalUniqueObjectMap.Empty();
	}

	StopCurrentActivity();
}

FNPCGoalContainer UNPCActivityComponent::GetGoals(const TSubclassOf<UNPCGoalItem>& GoalType) const
{
	if (Goals.Contains(GoalType))
	{
		return Goals[GoalType];
	}

	return FNPCGoalContainer();
}


bool UNPCActivityComponent::HasGoal(const TSubclassOf<UNPCGoalItem>& GoalType) const
{
	return GetGoals(GoalType).Goals.Num() > 0;
}

UNPCGoalItem* UNPCActivityComponent::GetGoalByKey(const TSubclassOf<UNPCGoalItem>& GoalType, const UObject* Key, bool& OutSucceeded)
{
	if (Goals.Contains(GoalType))
	{
		if (Goals[GoalType].GoalUniqueObjectMap.Contains(Key))
		{
			OutSucceeded = true;
			return Goals[GoalType].GoalUniqueObjectMap[Key];
		}
	}

	OutSucceeded = false; 
	return nullptr; 
}

void UNPCActivityComponent::StopActivity_Internal(UNPCActivity* Activity, bool bCleanupBT)
{
	if (Activity)
	{
		Activity->EndActivity();
		Activity->K2_EndActivity();

		if (bCleanupBT)
		{
			Activity->StopBehaviorTree();
		}
	}
}

void UNPCActivityComponent::Load_Implementation()
{
	//Add our activities back 
	for (auto& Activity : SavedActivities)
	{
		if (UNPCActivity* NewActivity = AddActivity(Activity.Class, true))
		{
			FMemoryReader MemReader(Activity.Data);
			FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
			Ar.ArIsSaveGame = true;

			NewActivity->Serialize(Ar);
		}
	}

	//Add our goalgens back
	for (auto& GoalGen : SavedGoalGenerators)
	{
		if (UNPCGoalGenerator* NewGoalGen = AddGoalGenerator(GoalGen.Class, true))
		{
			FMemoryReader MemReader(GoalGen.Data);
			FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
			Ar.ArIsSaveGame = true;

			NewGoalGen->Serialize(Ar);
		}
	}

	for (auto& SaveGoal : SavedGoals)
	{
		if (SaveGoal.Class)
		{
			if (UNPCGoalItem* NewGoal = NewObject<UNPCGoalItem>(this, SaveGoal.Class))
			{
				FMemoryReader MemReader(SaveGoal.Data);
				FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
				Ar.ArIsSaveGame = true;

				NewGoal->Serialize(Ar);

				NewGoal->OwnerController = OwnerController; 

				/*No one will have a pointer to the remade goal, but thats okay imo, stuff like followplayer can just 
				search the goals for it if it needs. */
				AddGoal(NewGoal);
			}
		}
	}

	//Now that our rescore interval has loaded, we need to restart the rescore ticker 
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle_RescoreGoals, this, &UNPCActivityComponent::RescoreGoals, RescoreInterval, true);
	}

	RescoreGoals();

}

void UNPCActivityComponent::PrepareForSave_Implementation()
{
	//Store all our goals and their SaveGame vars to our save record where we can read them back later 
	SavedGoals.Empty();
	SavedActivities.Empty();

	for (auto& Activity : Activities)
	{
		if (Activity && Activity->bSaveActivity)
		{
			FSavedNPCActivity SavedActivity;
			SavedActivity.Class = Activity->GetClass();

			FMemoryWriter MemWriter(SavedActivity.Data);
			FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
			Ar.ArIsSaveGame = true;

			Activity->Serialize(Ar);

			SavedActivities.Add(SavedActivity);
		}
	}

	for (auto& GoalGen : GoalGenerators)
	{
		if (GoalGen && GoalGen->bSaveGoalGenerator)
		{
			FSavedNPCGoalGenerator SavedGen;
			SavedGen.Class = GoalGen->GetClass();

			FMemoryWriter MemWriter(SavedGen.Data);
			FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
			Ar.ArIsSaveGame = true;

			GoalGen->Serialize(Ar);

			SavedGoalGenerators.Add(SavedGen);
		}
	}

	for (auto& GoalKVP : Goals)
	{
		FNPCGoalContainer& GoalSet = GoalKVP.Value;

		for (auto& Goal : GoalSet.Goals)
		{
			if (Goal && Goal->bSaveGoal)
			{
				FSavedGoalItem SavedGoal;
				SavedGoal.Class = Goal->GetClass();

				FMemoryWriter MemWriter(SavedGoal.Data);
				FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
				Ar.ArIsSaveGame = true;

				//Let the goal store any savegame data it needs first
				Goal->PrepareForSave();
				Goal->Serialize(Ar);

				SavedGoals.Add(SavedGoal);
			}
		}
	}


}

#if WITH_GAMEPLAY_DEBUGGER

FGameplayDebuggerCategory_ActivityComponent::FGameplayDebuggerCategory_ActivityComponent()
{

}

void FGameplayDebuggerCategory_ActivityComponent::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	//Debugging an NPC
	if (ANarrativeNPCCharacter* MyChar = Cast<ANarrativeNPCCharacter>(DebugActor))
	{
		ANarrativeNPCController* MyController = MyChar->GetNPCController();
		UNPCActivityComponent* ActivityComp = GetValid(MyController ? MyController->GetActivityComponent() : nullptr);

		if (ActivityComp)
		{
			ActivityComp->DescribeSelfToGameplayDebugger(this);
		}
	}
	else if(APawn* MyPawn = Cast<APawn>(DebugActor)) // Debugging something else like a car that the NPC has gotten into 
	{
		ANarrativeNPCController* MyController = Cast<ANarrativeNPCController>(MyPawn->GetController());
		UNPCActivityComponent* ActivityComp = GetValid(MyController ? MyController->GetActivityComponent() : nullptr);

		if (ActivityComp)
		{
			ActivityComp->DescribeSelfToGameplayDebugger(this);
		}
	}


}

void FGameplayDebuggerCategory_ActivityComponent::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	//CanvasContext.Printf(TEXT("CUSTOM ACTIVITY DRAW DATA"));
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ActivityComponent::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ActivityComponent());
}

#endif 