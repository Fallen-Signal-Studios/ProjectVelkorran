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
#include "UObject/StrongObjectPtr.h"

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
	if (bSavedActivityRestorePending)
	{
		// Actor::BeginPlay is still dispatching component callbacks here. A next
		// tick lets the real controller finish its own BeginPlay first.
		QueueSavedActivityRestore();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle_RescoreGoals, this, &UNPCActivityComponent::RescoreGoals, RescoreInterval, true);
	}

	RescoreGoals();
}

void UNPCActivityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bActivityComponentEndingPlay = true;
	RefreshGoalKeyActorBindings();
	bSavedActivityLoadAccepted = false;
	++SavedActivityLoadGeneration;
	bSavedActivityRestorePending = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_SavedActivityRestore);
		World->GetTimerManager().ClearTimer(TimerHandle_RescoreGoals);
	}
	OwnerController = nullptr;
	SavedActivityLoadController.Reset();
	SavedActivityLoadPawn.Reset();
	Super::EndPlay(EndPlayReason);
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
						if (HasStaleRegisteredGoalKey(Goal))
						{
							DebuggerCategory->AddTextLine(TEXT("   Expired object-key goal; awaiting ordinary selection cleanup."));
							continue;
						}
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
	if (!IsActive() || bSavedActivityRestorePending || bApplyingSavedActivities || bActivityComponentEndingPlay)
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
	if (bSavedActivityRestorePending || bApplyingSavedActivities || bActivityComponentEndingPlay)
	{
		FailReason = TEXT("Saved activity state is not initialized.");
		return false;
	}
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
		// The same published instance is not a new candidate. Reinitializing it
		// would replace its timer handles before uniqueness admission rejects it.
		if (const FNPCGoalContainer* Existing = Goals.Find(NewGoal->GetClass());
			Existing && Existing->Goals.Contains(NewGoal)) { return nullptr; }
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

		// A restoring generator can create ordinary goals from its initialization
		// callback. Do not publish one after that callback retires the snapshot.
		const bool bFromRestore = bApplyingSavedActivities;
		const uint64 Generation = SavedActivityLoadGeneration;
		NewGoal->Initialize();
		if (bFromRestore && !OwnsSavedActivityRestore(Generation)) { return nullptr; }
		UObject* GoalKey = NewGoal->GetGoalKey();
		if (bFromRestore && !OwnsSavedActivityRestore(Generation)) { return nullptr; }
		// Either outward callback may have published this exact instance in a
		// nested admission. Its accepted work belongs to that registration.
		if (const FNPCGoalContainer* Existing = Goals.Find(NewGoal->GetClass());
			Existing && Existing->Goals.Contains(NewGoal)) { return nullptr; }
		const AActor* ActorKey = Cast<AActor>(GoalKey);
		if (GoalKey && (!IsValid(GoalKey) || (ActorKey && ActorKey->IsActorBeingDestroyed())))
		{
			// Initialize may have started timers. Retire them through the normal
			// callback rather than publish a new goal for an already dying key.
			NewGoal->OnRemoved();
			return nullptr;
		}

		//Add the goal to the set.
		if (!Goals.Contains(NewGoal->GetClass()))
		{
			FNPCGoalContainer NewGoalSet;
			NewGoalSet.Goals.Add(NewGoal);

			if (UObject* Key = GoalKey)
			{
				NewGoalSet.GoalUniqueObjectMap.Add(Key, NewGoal);
			}

			Goals.Add(NewGoal->GetClass(), NewGoalSet);
		}
		else
		{	
			//Enforce uniqueness - dont allow multiple goals with same key
			if (UObject* Key = GoalKey)
			{
				if (Goals[NewGoal->GetClass()].GoalUniqueObjectMap.Contains(Key))
				{
					// Initialize already acquired timers/delegates. This distinct
					// rejected candidate will never receive registered-key cleanup.
					NewGoal->OnRemoved();
					return nullptr;
				}

				Goals[NewGoal->GetClass()].GoalUniqueObjectMap.Add(Key, NewGoal);
			}

			Goals[NewGoal->GetClass()].Goals.Add(NewGoal);

		}

		RefreshGoalKeyActorBindings();

		//let the goal set itself if needed - moved to start to key can be initialized 
		//NewGoal->Initialize();

		// The completed restore will score once after every saved row is applied.
		if (bFromRestore) { return NewGoal; }

		// A living target can be destroyed during a handoff without a death event.
		// Check its registered identity before either outward score callback.
		const auto ScoreRegisteredGoal = [this](const UNPCGoalItem* Goal)
		{ return HasStaleRegisteredGoalKey(Goal) ? -1.f : Goal->GetGoalScore(); };
		//By default if new goal scores higher than existing one lets perform a reselect
		if ((!CurrentActivity || !CurrentActivity->ActivityGoal) || (CurrentActivity && CurrentActivity->ActivityGoal && ScoreRegisteredGoal(NewGoal) > ScoreRegisteredGoal(CurrentActivity->ActivityGoal)))
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

void UNPCActivityComponent::RefreshGoalKeyActorBindings()
{
	TSet<TWeakObjectPtr<AActor>> RequiredActors;
	if (!bActivityComponentEndingPlay)
	{
		for (const auto& Pair : Goals)
		{
			for (const auto& Entry : Pair.Value.GoalUniqueObjectMap)
			{
				if (AActor* Actor = Cast<AActor>(Entry.Key); IsValid(Actor)
					&& Pair.Value.Goals.Contains(Entry.Value))
				{ RequiredActors.Add(Actor); }
			}
		}
	}
	for (const auto& WeakActor : BoundGoalKeyActors)
	{
		if (!RequiredActors.Contains(WeakActor))
		{
			if (AActor* Actor = WeakActor.Get())
			{ Actor->OnDestroyed.RemoveDynamic(this, &UNPCActivityComponent::OnGoalKeyActorDestroyed); }
		}
	}
	for (const auto& WeakActor : RequiredActors)
	{
		if (!BoundGoalKeyActors.Contains(WeakActor))
		{
			if (AActor* Actor = WeakActor.Get())
			{ Actor->OnDestroyed.AddUniqueDynamic(this, &UNPCActivityComponent::OnGoalKeyActorDestroyed); }
		}
	}
	BoundGoalKeyActors = MoveTemp(RequiredActors);
}

void UNPCActivityComponent::OnGoalKeyActorDestroyed(AActor* DestroyedActor)
{
	if (!DestroyedActor || bActivityComponentEndingPlay) { return; }
	// Cleanup callbacks may replace goals or load a newer snapshot. Keep no map
	// iterators across them, and remove only each captured exact registration.
	TArray<TWeakObjectPtr<UNPCGoalItem>> RetiringGoals;
	for (const auto& Pair : Goals)
	{
		for (const auto& Entry : Pair.Value.GoalUniqueObjectMap)
		{
			if (Entry.Key == DestroyedActor && Pair.Value.Goals.Contains(Entry.Value))
			{ RetiringGoals.AddUnique(Entry.Value); }
		}
	}
	const uint64 Generation = SavedActivityLoadGeneration;
	for (const auto& WeakGoal : RetiringGoals)
	{
		if (bActivityComponentEndingPlay || SavedActivityLoadGeneration != Generation) { return; }
		UNPCGoalItem* Goal = WeakGoal.Get();
		const FNPCGoalContainer* Container = Goal ? Goals.Find(Goal->GetClass()) : nullptr;
		const auto* Registered = Container ? Container->GoalUniqueObjectMap.Find(DestroyedActor) : nullptr;
		if (Registered && *Registered == Goal && Container->Goals.Contains(Goal))
		{ RemoveGoal(Goal); }
	}
}

bool UNPCActivityComponent::HasStaleRegisteredGoalKey(const UNPCGoalItem* Goal) const
{
	if (!IsValid(Goal)) { return true; }
	const FNPCGoalContainer* Container = Goals.Find(Goal->GetClass());
	// Removed goals may still be referenced by a current activity or a copied
	// scoring container while OnRemoved reenters. They are no longer admitted.
	if (!Container || !Container->Goals.Contains(Goal)) { return true; }
	for (const auto& Entry : Container->GoalUniqueObjectMap)
	{
		if (Entry.Value == Goal)
		{
			if (!IsValid(Entry.Key)) { return true; }
			const AActor* ActorKey = Cast<AActor>(Entry.Key);
			return ActorKey && ActorKey->IsActorBeingDestroyed();
		}
	}
	return false;
}

void UNPCActivityComponent::RemoveGoal(UNPCGoalItem* GoalToRemove)
{
	if (GoalToRemove)
	{
		if (Goals.Contains(GoalToRemove->GetClass()))
		{
			if (Goals[GoalToRemove->GetClass()].Goals.Remove(GoalToRemove) > 0)
			{
				// Remove this registration before the outward cleanup callback. The
				// original key may now be null/pending kill; never ask its Blueprint
				// getter again or erase a replacement registered by OnRemoved.
				for (auto It = Goals[GoalToRemove->GetClass()].GoalUniqueObjectMap.CreateIterator(); It; ++It)
				{ if (It.Value() == GoalToRemove) { It.RemoveCurrent(); } }
				RefreshGoalKeyActorBindings();
				GoalToRemove->OnRemoved();
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

	RefreshGoalKeyActorBindings();
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

bool UNPCActivityComponent::ValidateSaveRecord(const TArray<uint8>& RecordBytes) const
{
	// Detached decode invokes serialization only, never Load, BeginPlay or an
	// activity/goal initialization event. Reject malformed rows synchronously
	// before the save subsystem mutates the real actor/component.
	TStrongObjectPtr<UNPCActivityComponent> Decoded(NewObject<UNPCActivityComponent>(GetTransientPackage(), GetClass()));
	FMemoryReader Reader(RecordBytes);
	FObjectAndNameAsStringProxyArchive Ar(Reader, true);
	Ar.ArIsSaveGame = true;
	Decoded->Serialize(Ar);
	if (Ar.IsError() || Reader.Tell() != RecordBytes.Num()) { return false; }
	const auto ValidRows = [&Decoded](const auto& Records)
	{
		for (const auto& Record : Records)
		{
			if (!IsValid(Record.Class) || Record.Class->HasAnyClassFlags(CLASS_Abstract)) { return false; }
			TStrongObjectPtr<UObject> Object(NewObject<UObject>(Decoded.Get(), Record.Class));
			FMemoryReader RowReader(Record.Data);
			FObjectAndNameAsStringProxyArchive RowAr(RowReader, true);
			RowAr.ArIsSaveGame = true;
			Object->Serialize(RowAr);
			if (RowAr.IsError() || RowReader.Tell() != Record.Data.Num()) { return false; }
		}
		return true;
	};
	return ValidRows(Decoded->SavedActivities) && ValidRows(Decoded->SavedGoalGenerators) && ValidRows(Decoded->SavedGoals);
}

void UNPCActivityComponent::Load_Implementation()
{
	++SavedActivityLoadGeneration;
	bSavedActivityRestorePending = true;
	auto* Controller = Cast<ANarrativeNPCController>(GetOwner());
	SavedActivityLoadController = Controller;
	SavedActivityLoadPawn = Controller ? Controller->GetPawn() : nullptr;
	SavedActivityLoadPawnGeneration = Controller ? Controller->GetPawnAssignmentGeneration() : 0;
	bSavedActivityLoadAccepted = !bActivityComponentEndingPlay && IsValid(Controller)
		&& !Controller->IsActorBeingDestroyed() && Controller->HasAuthority()
		&& Controller->GetActivityComponent() == this && IsRegistered();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_RescoreGoals);
		World->GetTimerManager().ClearTimer(TimerHandle_SavedActivityRestore);
	}
	if (!bSavedActivityLoadAccepted) { return; }

	if (bApplyingSavedActivities || !HasBegunPlay() || !Controller->HasActorBegunPlay())
	{
		// BeginPlay schedules pre-initialization loads. A reentrant load on an
		// already initialized controller is applied on the next tick, never by
		// recursively continuing the retired snapshot's callback stack.
		if (HasBegunPlay()) { QueueSavedActivityRestore(); }
		return;
	}
	RestoreSavedActivities();
}

void UNPCActivityComponent::QueueSavedActivityRestore()
{
	if (!bSavedActivityRestorePending || !bSavedActivityLoadAccepted || bActivityComponentEndingPlay) { return; }
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_SavedActivityRestore);
		TimerHandle_SavedActivityRestore = World->GetTimerManager().SetTimerForNextTick(
			this, &UNPCActivityComponent::RestoreSavedActivities);
	}
}

bool UNPCActivityComponent::OwnsSavedActivityRestore(uint64 Generation) const
{
	const auto* Controller = SavedActivityLoadController.Get();
	return !bActivityComponentEndingPlay && IsRegistered() && HasBegunPlay()
		&& SavedActivityLoadGeneration == Generation && bSavedActivityLoadAccepted
		&& IsValid(Controller) && !Controller->IsActorBeingDestroyed() && Controller->HasActorBegunPlay()
		&& Controller->HasAuthority() && GetOwner() == Controller && OwnerController == Controller
		&& Controller->GetActivityComponent() == this && Controller->GetWorld() == GetWorld()
		&& Controller->GetPawnAssignmentGeneration() == SavedActivityLoadPawnGeneration
		&& !SavedActivityLoadPawn.IsStale() && Controller->GetPawn() == SavedActivityLoadPawn.Get();
}

void UNPCActivityComponent::RestoreSavedActivities()
{
	if (!bSavedActivityRestorePending || bApplyingSavedActivities) { return; }
	const uint64 Generation = SavedActivityLoadGeneration;
	if (!OwnsSavedActivityRestore(Generation))
	{
		bSavedActivityLoadAccepted = false;
		return;
	}

	// Copies prevent Blueprint callbacks that load a newer snapshot from
	// invalidating the active iteration or mixing the two sets of saved rows.
	const auto ActivityRecords = SavedActivities;
	const auto GeneratorRecords = SavedGoalGenerators;
	const auto GoalRecords = SavedGoals;
	const auto IsCurrent = [this, Generation]()
	{
		const bool bCurrent = OwnsSavedActivityRestore(Generation);
		if (!bCurrent && SavedActivityLoadGeneration == Generation) { bSavedActivityLoadAccepted = false; }
		return bCurrent;
	};
	const auto Deserialize = [this, &IsCurrent](UObject* Object, const TArray<uint8>& Data)
	{
		FMemoryReader Reader(Data);
		FObjectAndNameAsStringProxyArchive Ar(Reader, true);
		Ar.ArIsSaveGame = true;
		Object->Serialize(Ar);
		if (!IsCurrent()) { return false; }
		if (Ar.IsError()) { bSavedActivityLoadAccepted = false; return false; }
		return true;
	};
	{
		TGuardValue<bool> ApplyingGuard(bApplyingSavedActivities, true);
		// Retire the old execution before restoring its configuration. Publish
		// the null pointer before outward EndActivity callbacks can load again.
		UNPCActivity* PreviousActivity = CurrentActivity;
		CurrentActivity = nullptr;
		if (PreviousActivity)
		{
			PreviousActivity->EndActivity();
			if (!IsCurrent()) { return; }
			PreviousActivity->K2_EndActivity();
			if (!IsCurrent()) { return; }
			PreviousActivity->StopBehaviorTree();
			if (!IsCurrent()) { return; }
		}

		// Preserve ordinary unsaved generator goals. Replace only the saved
		// goals owned by the previous snapshot, using their normal removal event.
		TArray<TStrongObjectPtr<UNPCGoalItem>> PreviousSavedGoals;
		for (const auto& Pair : Goals)
		{
			for (UNPCGoalItem* Goal : Pair.Value.Goals)
			{
				if (IsValid(Goal) && Goal->bSaveGoal) { PreviousSavedGoals.Emplace(Goal); }
			}
		}
		for (const auto& Goal : PreviousSavedGoals)
		{
			if (auto* Container = Goals.Find(Goal->GetClass()))
			{
				Container->Goals.Remove(Goal.Get());
				for (auto It = Container->GoalUniqueObjectMap.CreateIterator(); It; ++It)
				{
					if (It.Value() == Goal.Get()) { It.RemoveCurrent(); }
				}
			}
			RefreshGoalKeyActorBindings();
			Goal->OnRemoved();
			if (!IsCurrent()) { return; }
		}

		for (const auto& Record : ActivityRecords)
		{
			if (!IsValid(Record.Class) || Record.Class->HasAnyClassFlags(CLASS_Abstract))
			{ bSavedActivityLoadAccepted = false; return; }
			UNPCActivity* Activity = GetActivity(Record.Class);
			if (!Activity) { Activity = AddActivity(Record.Class, true); }
			if (!Activity || !IsCurrent() || !Deserialize(Activity, Record.Data)) { return; }
			Activity->bSaveActivity = true;
		}

		// Older writers appended generator rows on every save. Use the latest
		// row for each class, initializing at most one actual generator instance.
		for (int32 Index = 0; Index < GeneratorRecords.Num(); ++Index)
		{
			const auto& Record = GeneratorRecords[Index];
			bool bHasLaterRecord = false;
			for (int32 Later = Index + 1; Later < GeneratorRecords.Num(); ++Later)
			{ bHasLaterRecord |= GeneratorRecords[Later].Class == Record.Class; }
			if (bHasLaterRecord) { continue; }
			if (!IsValid(Record.Class) || Record.Class->HasAnyClassFlags(CLASS_Abstract))
			{ bSavedActivityLoadAccepted = false; return; }
			UNPCGoalGenerator* Generator = GetGoalGenerator(Record.Class);
			const bool bNewGenerator = !Generator;
			if (bNewGenerator)
			{
				Generator = NewObject<UNPCGoalGenerator>(this, Record.Class);
				GoalGenerators.Add(Generator);
			}
			if (!Deserialize(Generator, Record.Data)) { return; }
			Generator->bSaveGoalGenerator = true;
			if (bNewGenerator)
			{
				// Its initialization event observes the saved settings and the
				// real initialized controller, rather than a temporary fake owner.
				Generator->Initialize(OwnerController, this);
				if (!IsCurrent()) { return; }
			}
		}

		for (const auto& Record : GoalRecords)
		{
			if (!IsValid(Record.Class) || Record.Class->HasAnyClassFlags(CLASS_Abstract))
			{ bSavedActivityLoadAccepted = false; return; }
			TStrongObjectPtr<UNPCGoalItem> Goal(NewObject<UNPCGoalItem>(this, Record.Class));
			if (!Deserialize(Goal.Get(), Record.Data)) { return; }
			Goal->OwnerController = OwnerController;
			// Retain the saved creation/TOD/expiry fields; AddGoal is the new-goal
			// entry point and intentionally stamps the current time instead.
			Goal->Initialize();
			if (!IsCurrent()) { return; }
			UObject* Key = Goal->GetGoalKey();
			if (!IsCurrent()) { return; }
			if (const FNPCGoalContainer* Existing = Goals.Find(Goal->GetClass());
				Existing && Existing->Goals.Contains(Goal.Get())) { continue; }
			const AActor* ActorKey = Cast<AActor>(Key);
			if (Key && (!IsValid(Key) || (ActorKey && ActorKey->IsActorBeingDestroyed())))
			{
				Goal->OnRemoved();
				if (!IsCurrent()) { return; }
				continue;
			}
			auto& Container = Goals.FindOrAdd(Goal->GetClass());
			if (Key && Container.GoalUniqueObjectMap.Contains(Key))
			{
				// A preserved generator goal may already own this key. Retire
				// only the deserialized candidate's initialized work, then fence
				// callbacks that replaced the active snapshot.
				Goal->OnRemoved();
				if (!IsCurrent()) { return; }
				continue;
			}
			Container.Goals.Add(Goal.Get());
			if (Key) { Container.GoalUniqueObjectMap.Add(Key, Goal.Get()); }
			RefreshGoalKeyActorBindings();
		}
		if (!IsCurrent()) { return; }
		bSavedActivityRestorePending = false;
	}
	if (!IsCurrent()) { return; }
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle_RescoreGoals, this,
			&UNPCActivityComponent::RescoreGoals, RescoreInterval, true);
	}
	RescoreGoals();
	// Scoring and behavior-tree start are authored callbacks too. A successful
	// deserialization cannot authorize completion after they retire this owner.
	if (!IsCurrent() && SavedActivityLoadGeneration == Generation)
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(TimerHandle_RescoreGoals); }
	}
}

void UNPCActivityComponent::PrepareForSave_Implementation()
{
	// A load may be accepted before BeginPlay, or from a callback during another
	// restore. The serialized rows remain authoritative until fully applied.
	if (bSavedActivityRestorePending || bApplyingSavedActivities) { return; }

	//Store all our goals and their SaveGame vars to our save record where we can read them back later
	SavedGoals.Empty();
	SavedActivities.Empty();
	SavedGoalGenerators.Empty();

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