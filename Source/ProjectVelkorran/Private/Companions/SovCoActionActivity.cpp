// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Companions/SovCoActionActivity.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovCoActionAnchor.h"
#include "AI/NarrativeNPCController.h"
#include "AITypes.h"
#include "Engine/World.h"

USovCoActionGoal::USovCoActionGoal(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSaveGoal = false;
	bRemoveOnSucceeded = true;
	DefaultScore = 10000.f;
	GoalLifetime = -1.f;
}

float USovCoActionGoal::GetGoalScore_Implementation() const
{
	return IsValid(Companion) && Companion->IsRequestCurrent(this) ? DefaultScore : -1.f;
}

bool USovCoActionGoal::ShouldCleanup_Implementation() const
{
	return !IsValid(Companion) || !Companion->IsRequestCurrent(this);
}

USovCoActionActivity::USovCoActionActivity(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SupportedGoalType = USovCoActionGoal::StaticClass();
	bIsInterruptable = false;
}

bool USovCoActionActivity::RunActivity()
{
	USovCoActionGoal* Goal = Cast<USovCoActionGoal>(ActivityGoal);
	if (!Goal || !IsValid(Goal->Companion) || !Goal->Companion->IsRequestCurrent(Goal)) { return false; }
	LastActivateTime = GetWorld()->GetTimeSeconds();
	if (!RestartOwnedMove()) { Goal->Companion->NotifyPathResult(Goal, false); }
	// Keep the existing activity slot until its component resolves the failed
	// path/fallback on the next tick; no synchronous goal-removal reentrancy.
	return true;
}

bool USovCoActionActivity::RestartOwnedMove()
{
	ReleaseOwnedMove();
	bResolved = false;
	USovCoActionGoal* Goal = Cast<USovCoActionGoal>(ActivityGoal);
	if (!OwnerController || !Goal || !Goal->Anchor || !Goal->Companion || !Goal->Companion->IsRequestCurrent(Goal)) { return false; }
	UPathFollowingComponent* Paths = OwnerController->GetPathFollowingComponent();
	if (!Paths) { return false; }
	PathFinishedHandle = Paths->OnRequestFinished.AddUObject(this, &ThisClass::OnPathFinished);
	FAIMoveRequest Request(Goal->Anchor->GetCompanionLocation());
	Request.SetAcceptanceRadius(Goal->Anchor->ReachRadius * 0.5f);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(false);
	Request.SetProjectGoalLocation(true);
	Request.SetReachTestIncludesAgentRadius(false);
	const FPathFollowingRequestResult Result = OwnerController->MoveTo(Request);
	MoveRequestId = Result.MoveId;
	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bResolved = true;
		Goal->Companion->NotifyPathResult(Goal, true);
		return true;
	}
	return Result.Code == EPathFollowingRequestResult::RequestSuccessful;
}

void USovCoActionActivity::OnPathFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID != MoveRequestId || bResolved) { return; }
	bResolved = true;
	if (USovCoActionGoal* Goal = Cast<USovCoActionGoal>(ActivityGoal))
	{
		if (IsValid(Goal->Companion)) { Goal->Companion->NotifyPathResult(Goal, Result.IsSuccess()); }
	}
}

void USovCoActionActivity::ReleaseOwnedMove()
{
	if (OwnerController)
	{
		if (UPathFollowingComponent* Paths = OwnerController->GetPathFollowingComponent())
		{
			Paths->OnRequestFinished.Remove(PathFinishedHandle);
			if (MoveRequestId.IsValid() && Paths->GetCurrentRequestId() == MoveRequestId) { OwnerController->StopMovement(); }
		}
	}
	MoveRequestId = FAIRequestID::InvalidRequest;
	PathFinishedHandle.Reset();
}

bool USovCoActionActivity::EndActivity()
{
	ReleaseOwnedMove();
	if (USovCoActionGoal* Goal = Cast<USovCoActionGoal>(ActivityGoal))
	{
		if (IsValid(Goal->Companion)) { Goal->Companion->NotifyActivityInterrupted(Goal); }
	}
	return true;
}

void USovCoActionActivity::StopBehaviorTree()
{
	// This activity owns one path request, not another activity's Behavior Tree.
	ReleaseOwnedMove();
}
