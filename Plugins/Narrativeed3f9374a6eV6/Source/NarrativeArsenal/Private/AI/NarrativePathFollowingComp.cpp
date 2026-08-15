// Copyright Narrative Tools 2025.


#include "AI/NarrativePathFollowingComp.h"

void UNarrativePathFollowingComp::OnPathFinished(const FPathFollowingResult& Result)
{
	CachedLastDestination = OriginalMoveRequestGoalLocation;

	Super::OnPathFinished(Result);
}
