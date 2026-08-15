// Copyright Narrative Tools 2025.


#include "AI/AITask_LongMove.h"

#include "AIController.h"
#include "AISystem.h"
#include "ArsenalStatics.h"
#include "Navigation/MapTileBounds.h"
#include "Navigation/NavigationSubsystem.h"
#include "Tasks/AITask_MoveTo.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY_STATIC(LogLongMove, Log, All);

UAITask_LongMove::UAITask_LongMove(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsPausable = true;

	FinalMoveRequest.SetAcceptanceRadius(GET_AI_CONFIG_VAR(AcceptanceRadius));
	FinalMoveRequest.SetReachTestIncludesAgentRadius(GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap));
	FinalMoveRequest.SetAllowPartialPath(GET_AI_CONFIG_VAR(bAcceptPartialPaths));
	FinalMoveRequest.SetUsePathfinding(true);
}

void UAITask_LongMove::Setup(AAIController* Controller, FVector GoalLocation, float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath, bool bLockAILogic, EAIOptionFlag::Type ProjectGoalOnNavigation, EAIOptionFlag::Type RequireNavigableEndLocation)
{
	FAIMoveRequest& MoveReq = FinalMoveRequest;
	FinalGoal = GoalLocation;

	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveReq.IsReachTestIncludingAgentRadius()));
	MoveReq.SetAllowPartialPath(FAISystem::PickAIOption(AcceptPartialPath, MoveReq.IsUsingPartialPaths()));
	MoveReq.SetUsePathfinding(true);
	MoveReq.SetProjectGoalLocation(FAISystem::PickAIOption(ProjectGoalOnNavigation, MoveReq.IsProjectingGoal()));
	MoveReq.SetRequireNavigableEndLocation(FAISystem::PickAIOption(RequireNavigableEndLocation, MoveReq.IsNavigableEndLocationRequired()));

	if (Controller)
	{
		MoveReq.SetNavigationFilter(Controller->GetDefaultNavigationFilterClass());
	}

	if (bLockAILogic)
	{
		RequestAILogicLocking();
	}
}

UAITask_LongMove* UAITask_LongMove::RunLongMove(AAIController* Controller, FVector GoalLocation, float AcceptanceRadius,
                                                EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath, bool bLockAILogic,
                                                EAIOptionFlag::Type ProjectGoalOnNavigation, EAIOptionFlag::Type RequireNavigableEndLocation)
{
	if (GoalLocation.IsZero()) { return nullptr; }
	if (!Controller) { return nullptr; }
	
	UAITask_LongMove* MyTask = NewAITask<UAITask_LongMove>(*Controller, EAITaskPriority::High);

	MyTask->Setup(Controller, GoalLocation, AcceptanceRadius, StopOnOverlap, AcceptPartialPath, bLockAILogic,
	      ProjectGoalOnNavigation, RequireNavigableEndLocation);

	return MyTask;
}

bool UAITask_LongMove::HasMoveFailed()
{
	return bRequestFailed;
}

bool UAITask_LongMove::CalculatePath(const FPOIData* StartingPOI, const FPOIData* EndingPOI, TArray<const FPOIData*>& OutPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAITask_LongMove::CalculatePath)

	// Heuristic will help guide A Star to pick the best node to traverse next
	auto Heuristic = [&EndingPOI](const FPOIData* A)
	{
		return FVector::Dist2D(A->POILocation, EndingPOI->POILocation);
	};

	// GScore is the current cost from start to current
	TMap<const FPOIData*, float> GScore;
	GScore.Add(StartingPOI, 0);

	// FScore is the approximate cost from current to end
	TMap<const FPOIData*, float> FScore;
	FScore.Add(StartingPOI, Heuristic(StartingPOI));

	// Sort method so that we pick the best node in the array
	auto FScoreSort = [&FScore](const FPOIData& A, const FPOIData& B)
	{
		return FScore[&A] < FScore[&B];
	};

	TArray<const FPOIData*> OpenSet;
	OpenSet.Heapify(FScoreSort);
	OpenSet.HeapPush(StartingPOI, FScoreSort);
	
	TMap<const FPOIData*, const FPOIData*> CameFrom;
	
	while (!OpenSet.IsEmpty())
	{
		const FPOIData* Current = nullptr;
		OpenSet.HeapPop(Current, FScoreSort);

		if (CameFrom.Contains(Current))
		{
			UE_VLOG_SEGMENT(this, LogLongMove, Log, Current->POILocation, CameFrom[Current]->POILocation, FColor::Red, TEXT(""));
		}

		// Reached goal
		if (Current->POITag == EndingPOI->POITag)
		{
			TArray<const FPOIData*> ReversedPath;
			
			// Reconstruct path by using CameFrom TMap
			while (CameFrom.Contains(Current))
			{
				ReversedPath.Emplace(Current);
				UE_VLOG_SEGMENT_THICK(this, LogLongMove, Log, Current->POILocation, CameFrom[Current]->POILocation, FColor::Green, 5.f, TEXT(""));
				Current = CameFrom[Current];
			}

			// Add starting POI
			ReversedPath.Emplace(Current);

			// Return the path but in the correct order (start is index 0)
			for (int i=ReversedPath.Num()-1; i >= 0; i--)
			{
				OutPath.Emplace(ReversedPath[i]);
			}
			
			return true;
		}

		for (const FGameplayTag& LinkedPOI : Current->LinkedPOIs)
		{
			// We need a pointer as this will be our key for the data structures
			FPOIData* Neighbor = NavigationSubsystem->POILookupMap.Find(LinkedPOI);
			if (!Neighbor) { continue; }

			float CurrentToNeighbor = FVector::Dist2D(Neighbor->POILocation, Current->POILocation);
			float CurrentGScore = GScore.Contains(Current) ? GScore[Current] : UE_MAX_FLT;
			float TentativeGScore = CurrentGScore + CurrentToNeighbor;

			float NeighborGScore = GScore.Contains(Neighbor) ? GScore[Neighbor] : UE_MAX_FLT;
			if (TentativeGScore < NeighborGScore)
			{
				CameFrom.Add(Neighbor, Current);
				GScore.Add(Neighbor, TentativeGScore);
				FScore.Add(Neighbor, TentativeGScore + Heuristic(Neighbor));
				if (!OpenSet.Contains(Neighbor))
				{
					UE_VLOG_BOX(this, LogLongMove, Log, FBox::BuildAABB(Neighbor->POILocation, FVector(200.f)), FColor::Red, TEXT(""));
					OpenSet.HeapPush(Neighbor, FScoreSort);
				}
			}
		}
	}
	// Unable to find path
	return false;
}

void UAITask_LongMove::MoveFinished(FAIRequestID FaiRequestID, const FPathFollowingResult& PathFollowingResult)
{
	if (FaiRequestID != RequestResult.MoveId) { return; }
	
	switch (PathFollowingResult.Code)
	{
	case EPathFollowingResult::Success:
		CurrentTargetIndex++;
		if (CurrentTargetIndex < Path.Num())
		{
			PerformMove(Path[CurrentTargetIndex]);
		}
		else
		{
			OnMoveFinished.Broadcast(PathFollowingResult.Code, OwnerController);
			EndTask();
		}
		break;
	default:
		FailLongMove();
		break;
	}
	
}

void UAITask_LongMove::PerformMove(const FVector& GoalLocation)
{
	FAIMoveRequest MoveRequest = FinalMoveRequest;
	MoveRequest.SetGoalLocation(GoalLocation);

	UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
	if (!PFComp)
	{
		FailLongMove();
		return;
	}

	PFComp->OnRequestFinished.Remove(RequestFinishedHandle); // Prevents unnecessary logic from running within MoveFinished during transition
	PFComp->AbortMove(*this, FPathFollowingResultFlags::NewRequest, RequestResult.MoveId, EPathFollowingVelocityMode::Keep);
	
	RequestResult = OwnerController->MoveTo(MoveRequest);

	switch (RequestResult.Code)
	{
	case EPathFollowingRequestResult::AlreadyAtGoal:
		OnMoveFinished.Broadcast(EPathFollowingResult::Success, OwnerController);
		EndTask();
		break;
	case EPathFollowingRequestResult::Failed:
		FailLongMove();
		break;
	case EPathFollowingRequestResult::RequestSuccessful:
		RequestFinishedHandle = PFComp->OnRequestFinished.AddUObject(this, &UAITask_LongMove::MoveFinished);
		break;
	default:
		break;
	}
}

bool UAITask_LongMove::GetPOIPoints(const FPOIData*& OutStartingPOI, const FPOIData*& OutEndingPOI)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAITask_LongMove::FindNearestPOIs)
	auto MapTileBounds = NavigationSubsystem->MapTileBounds;
	
	float ClosestStarting = UE_MAX_FLT;
	float ClosestEnding = UE_MAX_FLT;
	
	for (const FPOIData& POI : MapTileBounds->POIs)
	{
		float DistanceToPlayer = FVector::Dist2D(POI.POILocation, GetAvatarActor()->GetActorLocation());
		float DistanceToGoal = FVector::Dist2D(POI.POILocation, FinalGoal);

		// Closest POI to actor
		if (DistanceToPlayer < ClosestStarting)
		{
			OutStartingPOI = &POI;
			ClosestStarting = DistanceToPlayer;
		}

		// Closest POI to goal
		if (DistanceToGoal < ClosestEnding)
		{
			OutEndingPOI = &POI;
			ClosestEnding = DistanceToGoal;
		}
	}
	
	return OutStartingPOI && OutEndingPOI;
}

void UAITask_LongMove::Activate()
{
	Super::Activate();
	
	NavigationSubsystem = GetWorld()->GetSubsystem<UNavigationSubsystem>();
	if (!ensureMsgf(NavigationSubsystem, TEXT("Invalid NavigationSubsystem, unable to perform LongMove")))
	{
		FailLongMove();
		return;
	}

	if (!ensureMsgf(NavigationSubsystem->MapTileBounds, TEXT("Invalid MapTileBounds, unable to perform LongMove")))
	{
		FailLongMove();
		return;
	}

	UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
	if (!PFComp)
	{
		FailLongMove();
		return;
	}

	// Early out if we can navigate to the end goal without performing A Star
	if (UArsenalStatics::CheckPathExists(GetAvatarActor()->GetActorLocation(), FinalGoal, GetAvatarActor(), nullptr))
	{
		PerformMove(FinalGoal);
		return;
	}

	const FPOIData* StartingPOI = nullptr;
	const FPOIData* EndingPOI = nullptr;

	if (!GetPOIPoints(StartingPOI, EndingPOI))
	{
		UE_LOG(LogLongMove, Error, TEXT("Unable to perform long move: Could not find a valid Starting POI or Ending POI"))
		FailLongMove();
		return;
	}

	TArray<const FPOIData*> POIPath;
	if (CalculatePath(StartingPOI, EndingPOI, POIPath))
	{
		// Turn POIPoints into an array vector
		Path.Reserve(POIPath.Num() + 1);
		for (const FPOIData* POIPoint : POIPath)
		{
			Path.Emplace(POIPoint->POILocation);
		}

		Path.Emplace(FinalGoal);

		UpdateNavigablePOI();
		GetWorld()->GetTimerManager().SetTimer(UpdateNavigablePOIHandle, FTimerDelegate::CreateUObject(this, &UAITask_LongMove::UpdateNavigablePOI), UpdatePOIRate, true);

		// If we are not already pathing (as this can be done within UpdateNavigablePOI), do so now
		if (!RequestResult.MoveId.IsValid())
		{
			PerformMove(Path[CurrentTargetIndex]);
		}
	}
}

void UAITask_LongMove::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);

	UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
	if (PFComp)
	{
		PFComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, RequestResult.MoveId);	
	}

	if (OwnerController)
	{
		OwnerController->GetWorldTimerManager().ClearAllTimersForObject(this);
	}
}

void UAITask_LongMove::Pause()
{
	Super::Pause();

	OwnerController->PauseMove(RequestResult.MoveId);
}

void UAITask_LongMove::Resume()
{
	Super::Resume();

	// If the move was paused and cannot resume, we should attempt to start again
	if (!RequestResult.MoveId.IsValid() || (OwnerController && !OwnerController->ResumeMove(RequestResult.MoveId)))
	{
		PerformMove(Path[CurrentTargetIndex]);
	}
}

void UAITask_LongMove::FailLongMove()
{
	bRequestFailed = true;
	OnRequestFailed.Broadcast();
	EndTask();
}

void UAITask_LongMove::UpdateNavigablePOI()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAITask_LongMove::UpdateNavigablePOI)

	int OldTargetIndex = CurrentTargetIndex;
	
	// If there is a POI within the Path that is already navigable (other than the starting node), lets go there instead
	for (int i=CurrentTargetIndex;i<Path.Num();i++)
	{
		if (!UArsenalStatics::CheckPathExists(GetAvatarActor()->GetActorLocation(), Path[i], GetAvatarActor(), nullptr))
		{
			break;
		}
			
		CurrentTargetIndex = i;
	}

	// Send updated move if our target index is different
	if (OldTargetIndex != CurrentTargetIndex)
	{
		PerformMove(Path[CurrentTargetIndex]);
	}
}
