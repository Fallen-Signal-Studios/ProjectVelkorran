// Copyright Narrative Tools 2025.


#include "Navigation/GameplayTasks/GameplayTask_MoveToLocationAndRotationMulti.h"

#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogMoveToLocationAndRotationMulti, Log, All);

UGameplayTask_MoveToLocationAndRotationMulti*
UGameplayTask_MoveToLocationAndRotationMulti::MoveToLocationAndRotationMulti(
	TArray<ANarrativeCharacter*> Characters, const TArray<FTransform>& TargetTransforms,
	float InterpSpeed, EGoalReachMode GoalReachMode, bool bEndWhenFinishedMove)
{
	// Cannot create task if we have no task owners
	if (Characters.IsEmpty())
	{
		UE_LOG(LogMoveToLocationAndRotationMulti, Error, TEXT("Unable to create MoveToLocationAndRotationMulti without valid Characters"))
		return nullptr;
	}
	
	if (Characters.Num() != TargetTransforms.Num())
	{
		UE_LOG(LogMoveToLocationAndRotationMulti, Error, TEXT("TargetTransforms # does not match # of Characters! Characters: %d | TargetTransforms: %d"), Characters.Num(), TargetTransforms.Num());
		return nullptr;
	}
	
	// Cannot create gameplay task for task owners that are not IGameplayTaskOwnerInterface
	for (ANarrativeCharacter* Character : Characters)
	{
		if (!Character)
		{
			UE_LOG(LogMoveToLocationAndRotationMulti, Error, TEXT("Unable to create MoveToMultiTask: Invalid character found"))
			return nullptr;
		}
	}
	
	UWorld* World = GEngine->GetWorldFromContextObject(Characters[0], EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}
	
	auto MyObj = NewObject<UGameplayTask_MoveToLocationAndRotationMulti>();
	
	MyObj->Characters = Characters;
	MyObj->TargetTransforms = TargetTransforms;
	MyObj->InterpSpeed = InterpSpeed;
	MyObj->GoalReachMode = GoalReachMode;
	MyObj->bEndWhenFinishedMove = bEndWhenFinishedMove;
	MyObj->RegisterWithGameInstance(World);
	
	return MyObj;
}

void UGameplayTask_MoveToLocationAndRotationMulti::Activate()
{
	Super::Activate();
	
	for (auto It = Characters.CreateIterator(); It; ++It)
	{
		auto MoveToTask = UGameplayTask_MoveToLocationAndRotation::MoveToLocationAndRotation(*It, TargetTransforms[It.GetIndex()].GetLocation(), TargetTransforms[It.GetIndex()].Rotator(), InterpSpeed, GoalReachMode, bEndWhenFinishedMove);
		if (!MoveToTask)
		{
			UE_LOG(LogMoveToLocationAndRotationMulti, Error, TEXT("Unable to create MoveTo task for %s"), *GetNameSafe(*It));
			FailMoveToTask(EPathFollowingResult::Invalid);
			SetReadyToDestroy();
			return;
		}
		MoveToTasks.Add(MoveToTask);
		
		MoveToTask->OnCompleted.AddDynamic(this, &ThisClass::MoveTaskCompleted);
		MoveToTask->OnFailed.AddDynamic(this, &ThisClass::MoveTaskFailed);
		
		MoveToTask->ReadyForActivation();
	}
}

void UGameplayTask_MoveToLocationAndRotationMulti::MoveTaskCompleted(EPathFollowingResult::Type Result)
{
	if (Result == EPathFollowingResult::Success)
	{
		TasksCompleted++;
		
		if (TasksCompleted >= TargetTransforms.Num())
		{
			TaskCompleted.Broadcast(Result);
			SetReadyToDestroy();
		}
	}
	else
	{
		FailMoveToTask(Result);
	}
}

void UGameplayTask_MoveToLocationAndRotationMulti::FailMoveToTask(EPathFollowingResult::Type Result)
{
	for (UGameplayTask_MoveToLocationAndRotation* MoveToTask : MoveToTasks)
	{
		MoveToTask->OnCompleted.RemoveAll(this);
		MoveToTask->OnFailed.RemoveAll(this);
		MoveToTask->EndTask();
	}
		
	TaskFailed.Broadcast(Result);
	SetReadyToDestroy();
}

void UGameplayTask_MoveToLocationAndRotationMulti::MoveTaskFailed(EPathFollowingResult::Type Result)
{
	if (Result != EPathFollowingResult::Success)
	{
		FailMoveToTask(Result);
	}
}
