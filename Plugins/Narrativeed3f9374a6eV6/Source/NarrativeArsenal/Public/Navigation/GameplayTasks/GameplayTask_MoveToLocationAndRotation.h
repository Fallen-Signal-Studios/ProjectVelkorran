// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTask.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameplayTask_MoveToLocationAndRotation.generated.h"

class ANarrativeCharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoveRotateCompleted, EPathFollowingResult::Type, Result);

UENUM(BlueprintType)
enum class EGoalReachMode : uint8
{
	ExactLocation		UMETA(ToolTip="reach test uses only AcceptanceRadius"),
	OverlapAgent		UMETA(ToolTip="reach test uses AcceptanceRadius increased by modified agent radius"),
	OverlapGoal			UMETA(ToolTip="reach test uses AcceptanceRadius increased by goal actor radius"),
	OverlapAgentAndGoal UMETA(ToolTip="reach test uses AcceptanceRadius increased by modified agent radius AND goal actor radius"),
};

/**
 * Moves to the specified location and then applies the set rotation to the actor. 
 */
UCLASS()
class NARRATIVEARSENAL_API UGameplayTask_MoveToLocationAndRotation : public UGameplayTask
{
	GENERATED_BODY()

public:
	UGameplayTask_MoveToLocationAndRotation(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "TaskOwner", AutoCreateRefTerm="TargetLocation,TargetRotation", BlueprintInternalUseOnly = "TRUE", AdvancedDisplay="GoalReachMode"))
	static UGameplayTask_MoveToLocationAndRotation* MoveToLocationAndRotation(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, const FVector& TargetLocation, const FRotator& TargetRotation, float InterpSpeed, EGoalReachMode GoalReachMode = EGoalReachMode::ExactLocation, bool bEndWhenFinishedMove=true);

	static UGameplayTask_MoveToLocationAndRotation* MoveToLocationAndRotation(ANarrativeCharacter* Character, const FVector& TargetLocation, const FRotator& TargetRotation, float InterpSpeed, EGoalReachMode GoalReachMode = EGoalReachMode::ExactLocation, bool bEndWhenFinishedMove=true);
	
	bool HasFinishedMove() const;

protected:
	void FinishMove(FAIRequestID FailRequestID, const FPathFollowingResult& PathFollowingResult);
	virtual void Activate() override;
	virtual void OnDestroy(bool AbilityEnding) override;
	virtual void TickTask(float DeltaTime) override;

	void FinishedRotating();

	// Movement
	bool bStartedMoving = false; 
	bool bFinishedMoving = false;
	bool bFinishedRotating = false;
	FDelegateHandle FinishMoveHandle;
	FVector DesiredLocation;
	FAIRequestID MoveRequestID = FAIRequestID::InvalidRequest;
	EGoalReachMode GoalReachMode;

	UPROPERTY()
	UPathFollowingComponent* PFComp = nullptr;

	// Rotation
	FRotator DesiredRotation;
	float RotationInterpSpeed;
	bool bEndTaskWhenFinishedMove;
	
	// Client Polling
	float ClientRotationTolerance = 10.f;
	float ClientLocationTolerance = 10.f;

public:
	// Delegates
	UPROPERTY(BlueprintAssignable)
	FMoveRotateCompleted OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FMoveRotateCompleted OnFailed;
	
};
