// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Navigation/NavigationSubsystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Tasks/AITask.h"
#include "Tasks/AITask_MoveTo.h"
#include "AITask_LongMove.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMoveTaskCompleted, TEnumAsByte<EPathFollowingResult::Type>, Result, AAIController*, AIController);

UCLASS()
class NARRATIVEARSENAL_API UAITask_LongMove : public UAITask
{
	GENERATED_BODY()

public:
	UAITask_LongMove(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void Setup(AAIController* Controller, FVector GoalLocation, float AcceptanceRadius,
	              EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath, bool bLockAILogic,
	              EAIOptionFlag::Type ProjectGoalOnNavigation, EAIOptionFlag::Type RequireNavigableEndLocation);

	/**
	 * Use linked POIs to determine a path to the destination even when the navmesh may not completely exist yet.
	 * If a path does exist, we fall back to the traditional AITask_MoveTo task.
	 * @param Controller The AI controller performing the movement
	 * @param GoalLocation The target location for the agent
	 * @param AcceptanceRadius The range around the goal to complete the move task
	 * @param StopOnOverlap Whether the agent should complete the move when overlapping with the goal
	 * @param AcceptPartialPath Whether to accept a partial path to the goal
	 * @param bLockAILogic Whether AI logic should be locked while performing the move
	 * @param ProjectGoalOnNavigation Determines if we should project the goal location to the navmesh
	 * @param RequireNavigableEndLocation Determines if we need the end location to be navigable in order to execute
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuousGoalTracking,ProjectGoalOnNavigation,RequireNavigableEndLocation", DefaultToSelf = "Controller", BlueprintInternalUseOnly = "TRUE", DisplayName = "Long Range Move To Location"))
	static UAITask_LongMove* RunLongMove(AAIController* Controller, FVector GoalLocation,
		float AcceptanceRadius = -1.f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default,
		bool bLockAILogic = true, EAIOptionFlag::Type ProjectGoalOnNavigation = EAIOptionFlag::Default,
		EAIOptionFlag::Type RequireNavigableEndLocation = EAIOptionFlag::Default);

	bool HasMoveFailed();

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FMoveTaskCompleted OnMoveFinished;

protected:
	// Use A Star pathfinding to calculate path between starting POI and ending POI
	virtual bool CalculatePath(const FPOIData* StartingPOI, const FPOIData* EndingPOI, TArray<const FPOIData*>& OutPath);
	
	virtual void MoveFinished(FAIRequestID FaiRequestID, const FPathFollowingResult& PathFollowingResult);
	virtual void PerformMove(const FVector& GoalLocation);
	virtual bool GetPOIPoints(const FPOIData*& OutStartingPOI, const FPOIData*& OutEndingPOI);
	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual void Pause() override;
	virtual void Resume() override;
	void FailLongMove();

	// Attempts to skip POIs if future points are within the current navmesh
	virtual void UpdateNavigablePOI();

	// Generated move request that will be reused for move requests in MoveTo functions
	FAIMoveRequest FinalMoveRequest;

	UPROPERTY()
	UNavigationSubsystem* NavigationSubsystem;

	// The generated path between POI points
	TArray<FVector> Path;

	// The current index within the Path array we are navigating to
	int CurrentTargetIndex = 0;

	// The final goal to navigate to
	FVector FinalGoal;

	FPathFollowingRequestResult RequestResult = FPathFollowingRequestResult();

	// Handle for updating POIs during navigation
	FTimerHandle UpdateNavigablePOIHandle;

	// Rate, in seconds, to check for POI nav updates
	const float UpdatePOIRate = 1.f;

	FDelegateHandle RequestFinishedHandle;

	bool bRequestFailed = false;
};
