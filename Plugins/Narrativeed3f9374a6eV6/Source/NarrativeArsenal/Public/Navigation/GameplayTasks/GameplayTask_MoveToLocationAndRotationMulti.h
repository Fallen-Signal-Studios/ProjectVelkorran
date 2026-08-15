// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTask.h"
#include "GameplayTask_MoveToLocationAndRotation.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GameplayTask_MoveToLocationAndRotationMulti.generated.h"

class ANarrativeCharacter;
/**
 * Helper task for managing multiple move to location tasks
 */
UCLASS()
class NARRATIVEARSENAL_API UGameplayTask_MoveToLocationAndRotationMulti : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AutoCreateRefTerm="TargetTransforms", BlueprintInternalUseOnly = "TRUE", AdvancedDisplay="GoalReachMode"))
	static UGameplayTask_MoveToLocationAndRotationMulti* MoveToLocationAndRotationMulti(
		TArray<ANarrativeCharacter*> Characters, const TArray<FTransform>& TargetTransforms,
		float InterpSpeed, EGoalReachMode GoalReachMode = EGoalReachMode::ExactLocation,
		bool bEndWhenFinishedMove = true);
	
	virtual void Activate() override;
	
	
	UPROPERTY(BlueprintAssignable)
	FMoveRotateCompleted TaskCompleted;
	
	UPROPERTY(BlueprintAssignable)
	FMoveRotateCompleted TaskFailed;

protected:
	UFUNCTION()
	void MoveTaskCompleted(EPathFollowingResult::Type Result);
	
	UFUNCTION()
	void FailMoveToTask(EPathFollowingResult::Type Result);

	UFUNCTION()
	void MoveTaskFailed(EPathFollowingResult::Type Result);
	
	UPROPERTY()
	TArray<ANarrativeCharacter*> Characters;
	
	UPROPERTY()
	TArray<FTransform> TargetTransforms;
	
	UPROPERTY()
	float InterpSpeed;
	
	UPROPERTY()
	EGoalReachMode GoalReachMode;
	
	UPROPERTY()
	bool bEndWhenFinishedMove;
	
private:
	UPROPERTY()
	TArray<UGameplayTask_MoveToLocationAndRotation*> MoveToTasks;
	
	UPROPERTY()
	int TasksCompleted = 0;
};
